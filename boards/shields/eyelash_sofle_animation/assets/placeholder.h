#ifndef PLACEHOLDER_IMAGES_H
#define PLACEHOLDER_IMAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

/* -------- Image Descriptor Declarations -------- */
LV_IMG_DECLARE(placeholder_00);
LV_IMG_DECLARE(placeholder_01);

/* -------- Array of Frame Pointers -------- */
static const lv_img_dsc_t *placeholder_images[] = {
    &placeholder_00,
    &placeholder_01,
};

#define PLACEHOLDER_IMAGES_NUM 2

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PLACEHOLDER_IMAGES_H */
