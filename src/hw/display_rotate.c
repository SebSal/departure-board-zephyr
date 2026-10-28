/* Presents a monochrome display rotated 90 degrees clockwise.
 *
 * The e-ink panel scans portrait but the UI is landscape, and nothing below can turn it: uc81xx
 * implements no set_orientation, and LVGL's own rotation paths all skip I1, the colour format a
 * 1 bit panel forces. So this device sits in between. It reports the rotated geometry upwards,
 * collects writes into a panel shaped frame, and pushes that frame down once the frame is done.
 *
 * Both buffers are 1 bit per pixel packed along x with the leftmost pixel in the MSB, which is
 * what SCREEN_INFO_MONO_MSB_FIRST without SCREEN_INFO_MONO_VTILED means. Rotating is therefore a
 * bit level transpose rather than a copy. The frame persists between writes, so a partial update
 * only has to fill in the part that changed.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT app_display_rotate

LOG_MODULE_REGISTER(app_display_rotate, CONFIG_APP_DISPLAY_ROTATE_LOG_LEVEL);

struct display_rotate_config {
	const struct device *panel;
	/* As presented upwards, i.e. the panel's own dimensions swapped. */
	uint16_t width;
	uint16_t height;
	uint8_t *frame;
	size_t frame_size;

	/* Partial refreshes between two full ones, 0 when the panel has no partial profile or the
	 * periodic clear is off.
	 */
	uint32_t full_refresh_interval;
};

struct display_rotate_data {
	/* Refreshes on the partial profile since the last one on the full profile. */
	uint32_t partial_refreshes;
};

/* Panel geometry is the rotated geometry turned back around. */
#define PANEL_WIDTH(cfg)  ((cfg)->height)
#define PANEL_HEIGHT(cfg) ((cfg)->width)
#define STRIDE_BYTES(w)   ((w) / 8U)

static void frame_set_pixel(const struct display_rotate_config *cfg, uint16_t x, uint16_t y,
							bool on)
{
	const size_t index = (size_t)y * STRIDE_BYTES(PANEL_WIDTH(cfg)) + (x / 8U);
	const uint8_t mask = (uint8_t)BIT(7U - (x % 8U));

	if (on) {
		cfg->frame[index] |= mask;
	} else {
		cfg->frame[index] &= (uint8_t)~mask;
	}
}

static int display_rotate_write(const struct device *dev, uint16_t x, uint16_t y,
								const struct display_buffer_descriptor *desc, const void *buf)
{
	const struct display_rotate_config *cfg = dev->config;
	const uint8_t *src = buf;

	/* Each row is packed from the rect's own left edge, so a rect not starting on a byte
	 * boundary would shift every source bit. The mono rounder keeps x aligned; this only
	 * guards against a caller that does not.
	 */
	if ((x % 8U) != 0U || (desc->pitch % 8U) != 0U) {
		LOG_ERR("Unaligned write at x=%u pitch=%u", x, desc->pitch);
		return -ENOTSUP;
	}

	if ((x + desc->width) > cfg->width || (y + desc->height) > cfg->height) {
		LOG_ERR("Write %ux%u at (%u,%u) exceeds %ux%u", desc->width, desc->height, x, y, cfg->width,
				cfg->height);
		return -EINVAL;
	}

	const uint16_t src_stride = STRIDE_BYTES(desc->pitch);

	for (uint16_t row = 0; row < desc->height; row++) {
		for (uint16_t col = 0; col < desc->width; col++) {
			const bool on =
				(src[(size_t)row * src_stride + (col / 8U)] & BIT(7U - (col % 8U))) != 0U;

			/* Clockwise: the landscape top left corner becomes the panel's top
			 * right, so x runs down the panel and y runs back across it.
			 */
			frame_set_pixel(cfg, (uint16_t)(cfg->height - 1U - (y + row)), (uint16_t)(x + col), on);
		}
	}

	if (desc->frame_incomplete) {
		return 0;
	}

	const struct display_buffer_descriptor panel_desc = {
		.buf_size = cfg->frame_size,
		.width = PANEL_WIDTH(cfg),
		.height = PANEL_HEIGHT(cfg),
		.pitch = PANEL_WIDTH(cfg),
		.frame_incomplete = false,
	};

	/* The panel's partial profile only moves what changed, so it never resets the particle
	 * distribution and ghosting builds up behind it. Every so often one refresh goes through
	 * the full profile instead, which drives every pixel and clears that. uc81xx refreshes on
	 * the partial profile for a write while not blanked, and on the full one from blanking_off
	 * after blanking_on, so the clear is the frame written between the two.
	 */
	struct display_rotate_data *data = dev->data;

	if (cfg->full_refresh_interval == 0 || data->partial_refreshes < cfg->full_refresh_interval) {
		data->partial_refreshes++;
		return display_write(cfg->panel, 0, 0, &panel_desc, cfg->frame);
	}

	data->partial_refreshes = 0;

	int ret = display_blanking_on(cfg->panel);

	if (ret == 0) {
		ret = display_write(cfg->panel, 0, 0, &panel_desc, cfg->frame);
	}

	/* Even after a failed write, so the next one is not stuck on a blanked panel. */
	int ret_off = display_blanking_off(cfg->panel);

	return ret != 0 ? ret : ret_off;
}

static void display_rotate_get_capabilities(const struct device *dev,
											struct display_capabilities *caps)
{
	const struct display_rotate_config *cfg = dev->config;

	display_get_capabilities(cfg->panel, caps);
	caps->x_resolution = cfg->width;
	caps->y_resolution = cfg->height;
}

static int display_rotate_set_pixel_format(const struct device *dev,
										   const enum display_pixel_format pf)
{
	const struct display_rotate_config *cfg = dev->config;

	return display_set_pixel_format(cfg->panel, pf);
}

static int display_rotate_blanking_on(const struct device *dev)
{
	const struct display_rotate_config *cfg = dev->config;

	return display_blanking_on(cfg->panel);
}

static int display_rotate_blanking_off(const struct device *dev)
{
	const struct display_rotate_config *cfg = dev->config;

	/* A full refresh, since uc81xx starts out blanked on its full profile. An e-ink panel keeps
	 * its image across a reset while the driver comes up assuming a white one, and only a full
	 * refresh reconciles the two. This runs once, from main(), before anything is drawn.
	 */
	return display_blanking_off(cfg->panel);
}

static DEVICE_API(display, display_rotate_api) = {
	.blanking_on = display_rotate_blanking_on,
	.blanking_off = display_rotate_blanking_off,
	.write = display_rotate_write,
	.get_capabilities = display_rotate_get_capabilities,
	.set_pixel_format = display_rotate_set_pixel_format,
};

#define PANEL_NODE(inst) DT_INST_PHANDLE(inst, display)

/* Without a partial profile every refresh is a full one, so there is nothing to clear. */
#define PANEL_HAS_PARTIAL(inst) DT_NODE_EXISTS(DT_CHILD(PANEL_NODE(inst), partial))

/* The node's width and height are what LVGL sizes its buffers from, so they have to match the
 * panel's turned around. The panel's width becomes this node's height, so the panel must also be
 * byte aligned across its own scan direction for the transpose to land on whole bytes.
 */
#define DISPLAY_ROTATE_DEFINE(inst)                                                                \
	BUILD_ASSERT(DT_INST_PROP(inst, width) == DT_PROP(PANEL_NODE(inst), height) &&                 \
					 DT_INST_PROP(inst, height) == DT_PROP(PANEL_NODE(inst), width),               \
				 "rotated size must be the panel's size swapped");                                 \
	BUILD_ASSERT(DT_INST_PROP(inst, height) % 8 == 0, "rotated height must be a multiple of 8");   \
                                                                                                   \
	static uint8_t                                                                                 \
		display_rotate_frame_##inst[DT_INST_PROP(inst, width) * DT_INST_PROP(inst, height) / 8];   \
                                                                                                   \
	static const struct display_rotate_config display_rotate_config_##inst = {                     \
		.panel = DEVICE_DT_GET(PANEL_NODE(inst)),                                                  \
		.width = DT_INST_PROP(inst, width),                                                        \
		.height = DT_INST_PROP(inst, height),                                                      \
		.frame = display_rotate_frame_##inst,                                                      \
		.frame_size = sizeof(display_rotate_frame_##inst),                                         \
		.full_refresh_interval =                                                                   \
			PANEL_HAS_PARTIAL(inst) ? DT_INST_PROP(inst, full_refresh_interval) : 0,               \
	};                                                                                             \
                                                                                                   \
	static struct display_rotate_data display_rotate_data_##inst;                                  \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, &display_rotate_data_##inst,                           \
						  &display_rotate_config_##inst, POST_KERNEL,                              \
						  CONFIG_DISPLAY_INIT_PRIORITY, &display_rotate_api);

DT_INST_FOREACH_STATUS_OKAY(DISPLAY_ROTATE_DEFINE)
