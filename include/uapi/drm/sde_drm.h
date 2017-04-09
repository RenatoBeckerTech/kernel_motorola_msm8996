#ifndef _SDE_DRM_H_
#define _SDE_DRM_H_

/*
 * Each top level structure is of the following format:
 *
 * struct {
 *         uint64_t version;
 *         union {
 *                 struct version v1;
 *                 ...
 *         } u;
 *
 * Each top level structure maintains independent versioning and is defined
 * as follows:
 *
 * #define STRUCTNAME_V1	0x1
 * ...
 * #define STRUCTNAME_Vn	0x###
 * #define STRUCTNAME_VERSION	STRUCTNAME_Vn
 *
 * Version fields should ALWAYS be declared as type uint64_t. This is because
 * 64-bit compilers tend to pad the structure to 64-bit align the start of
 * union structure members. Having an explicit 64-bit version helps to maintain
 * consistent structure layout between 32-bit and 64-bit compilers.
 *
 * Updates to the structures UAPI should always define a new sub-structure to
 * place within the union, and update STRUCTNAME_VERSION to reference the
 * new version number.
 *
 * User mode code should always set the 'version' field to STRUCTNAME_VERSION.
 */

/* Total number of supported color planes */
#define SDE_MAX_PLANES  4

/* Total number of parameterized detail enhancer mapping curves */
#define SDE_MAX_DE_CURVES 3

 /* Y/RGB and UV filter configuration */
#define FILTER_EDGE_DIRECTED_2D		0x0
#define FILTER_CIRCULAR_2D		0x1
#define FILTER_SEPARABLE_1D		0x2
#define FILTER_BILINEAR			0x3

/* Alpha filters */
#define FILTER_ALPHA_DROP_REPEAT	0x0
#define FILTER_ALPHA_BILINEAR		0x1
#define FILTER_ALPHA_2D			0x3

/* Blend filters */
#define FILTER_BLEND_CIRCULAR_2D	0x0
#define FILTER_BLEND_SEPARABLE_1D	0x1

/* LUT configuration flags */
#define SCALER_LUT_SWAP			0x1
#define SCALER_LUT_DIR_WR		0x2
#define SCALER_LUT_Y_CIR_WR		0x4
#define SCALER_LUT_UV_CIR_WR		0x8
#define SCALER_LUT_Y_SEP_WR		0x10
#define SCALER_LUT_UV_SEP_WR		0x20

/**
 * Blend operations for "blend_op" property
 *
 * @SDE_DRM_BLEND_OP_NOT_DEFINED:   No blend operation defined for the layer.
 * @SDE_DRM_BLEND_OP_OPAQUE:        Apply a constant blend operation. The layer
 *                                  would appear opaque in case fg plane alpha
 *                                  is 0xff.
 * @SDE_DRM_BLEND_OP_PREMULTIPLIED: Apply source over blend rule. Layer already
 *                                  has alpha pre-multiplication done. If the fg
 *                                  plane alpha is less than 0xff, apply
 *                                  modulation as well. This operation is
 *                                  intended on layers having alpha channel.
 * @SDE_DRM_BLEND_OP_COVERAGE:      Apply source over blend rule. Layer is not
 *                                  alpha pre-multiplied. Apply
 *                                  pre-multiplication. If fg plane alpha is
 *                                  less than 0xff, apply modulation as well.
 * @SDE_DRM_BLEND_OP_MAX:           Used to track maximum blend operation
 *                                  possible by mdp.
 */
#define SDE_DRM_BLEND_OP_NOT_DEFINED    0
#define SDE_DRM_BLEND_OP_OPAQUE         1
#define SDE_DRM_BLEND_OP_PREMULTIPLIED  2
#define SDE_DRM_BLEND_OP_COVERAGE       3
#define SDE_DRM_BLEND_OP_MAX            4

/**
 * Bit masks for "src_config" property
 * construct bitmask via (1UL << SDE_DRM_<flag>)
 */
#define SDE_DRM_DEINTERLACE         0   /* Specifies interlaced input */

/* DRM bitmasks are restricted to 0..63 */
#define SDE_DRM_BITMASK_COUNT       64

/**
 * struct sde_drm_pix_ext_v1 - version 1 of pixel ext structure
 * @num_ext_pxls_lr: Number of total horizontal pixels
 * @num_ext_pxls_tb: Number of total vertical lines
 * @left_ftch:       Number of extra pixels to overfetch from left
 * @right_ftch:      Number of extra pixels to overfetch from right
 * @top_ftch:        Number of extra lines to overfetch from top
 * @btm_ftch:        Number of extra lines to overfetch from bottom
 * @left_rpt:        Number of extra pixels to repeat from left
 * @right_rpt:       Number of extra pixels to repeat from right
 * @top_rpt:         Number of extra lines to repeat from top
 * @btm_rpt:         Number of extra lines to repeat from bottom
 */
struct sde_drm_pix_ext_v1 {
	/*
	 * Number of pixels ext in left, right, top and bottom direction
	 * for all color components.
	 */
	int32_t num_ext_pxls_lr[SDE_MAX_PLANES];
	int32_t num_ext_pxls_tb[SDE_MAX_PLANES];

	/*
	 * Number of pixels needs to be overfetched in left, right, top
	 * and bottom directions from source image for scaling.
	 */
	int32_t left_ftch[SDE_MAX_PLANES];
	int32_t right_ftch[SDE_MAX_PLANES];
	int32_t top_ftch[SDE_MAX_PLANES];
	int32_t btm_ftch[SDE_MAX_PLANES];
	/*
	 * Number of pixels needs to be repeated in left, right, top and
	 * bottom directions for scaling.
	 */
	int32_t left_rpt[SDE_MAX_PLANES];
	int32_t right_rpt[SDE_MAX_PLANES];
	int32_t top_rpt[SDE_MAX_PLANES];
	int32_t btm_rpt[SDE_MAX_PLANES];

};

/**
 * Enable mask bits for "scaler" property
 *
 * @SDE_DRM_SCALER_PIX_EXT: pix ext sub-structures are valid
 * @SDE_DRM_SCALER_SCALER_2: scaler 2 sub-structures are valid
 * @SDE_DRM_SCALER_SCALER_3: scaler 3 sub-structures are valid
 * @SDE_DRM_SCALER_DECIMATE: decimation fields are valid
 */
#define SDE_DRM_SCALER_PIX_EXT      0x1
#define SDE_DRM_SCALER_SCALER_2     0x2
#define SDE_DRM_SCALER_SCALER_3     0x4
#define SDE_DRM_SCALER_DECIMATE     0x8

/**
 * struct sde_drm_scaler_v1 - version 1 of struct sde_drm_scaler
 * @lr:            Pixel extension settings for left/right
 * @tb:            Pixel extension settings for top/botton
 * @init_phase_x:  Initial scaler phase values for x
 * @phase_step_x:  Phase step values for x
 * @init_phase_y:  Initial scaler phase values for y
 * @phase_step_y:  Phase step values for y
 * @horz_filter:   Horizontal filter array
 * @vert_filter:   Vertical filter array
 */
struct sde_drm_scaler_v1 {
	/*
	 * General definitions
	 */
	uint32_t enable;

	/*
	 * Pix ext settings
	 */
	struct sde_drm_pix_ext_v1 pe;

	/*
	 * Decimation settings
	 */
	uint32_t horz_decimate;
	uint32_t vert_decimate;

	/*
	 * Phase settings
	 */
	int32_t init_phase_x[SDE_MAX_PLANES];
	int32_t phase_step_x[SDE_MAX_PLANES];
	int32_t init_phase_y[SDE_MAX_PLANES];
	int32_t phase_step_y[SDE_MAX_PLANES];

	/*
	 * Filter type to be used for scaling in horizontal and vertical
	 * directions
	 */
	uint32_t horz_filter[SDE_MAX_PLANES];
	uint32_t vert_filter[SDE_MAX_PLANES];
};

/**
 * struct sde_drm_de_v1 - version 1 of detail enhancer structure
 * @enable:         Enables/disables detail enhancer
 * @sharpen_level1: Sharpening strength for noise
 * @sharpen_level2: Sharpening strength for context
 * @clip:           Clip coefficient
 * @limit:          Detail enhancer limit factor
 * @thr_quiet:      Quite zone threshold
 * @thr_dieout:     Die-out zone threshold
 * @thr_low:        Linear zone left threshold
 * @thr_high:       Linear zone right threshold
 * @prec_shift:     Detail enhancer precision
 * @adjust_a:       Mapping curves A coefficients
 * @adjust_b:       Mapping curves B coefficients
 * @adjust_c:       Mapping curves C coefficients
 */
struct sde_drm_de_v1 {
	uint32_t enable;
	int16_t sharpen_level1;
	int16_t sharpen_level2;
	uint16_t clip;
	uint16_t limit;
	uint16_t thr_quiet;
	uint16_t thr_dieout;
	uint16_t thr_low;
	uint16_t thr_high;
	uint16_t prec_shift;
	int16_t adjust_a[SDE_MAX_DE_CURVES];
	int16_t adjust_b[SDE_MAX_DE_CURVES];
	int16_t adjust_c[SDE_MAX_DE_CURVES];
};

/**
 * struct sde_drm_scaler_v2 - version 2 of struct sde_drm_scaler
 * @enable:            Scaler enable
 * @dir_en:            Detail enhancer enable
 * @pe:                Pixel extension settings
 * @horz_decimate:     Horizontal decimation factor
 * @vert_decimate:     Vertical decimation factor
 * @init_phase_x:      Initial scaler phase values for x
 * @phase_step_x:      Phase step values for x
 * @init_phase_y:      Initial scaler phase values for y
 * @phase_step_y:      Phase step values for y
 * @preload_x:         Horizontal preload value
 * @preload_y:         Vertical preload value
 * @src_width:         Source width
 * @src_height:        Source height
 * @dst_width:         Destination width
 * @dst_height:        Destination height
 * @y_rgb_filter_cfg:  Y/RGB plane filter configuration
 * @uv_filter_cfg:     UV plane filter configuration
 * @alpha_filter_cfg:  Alpha filter configuration
 * @blend_cfg:         Selection of blend coefficients
 * @lut_flag:          LUT configuration flags
 * @dir_lut_idx:       2d 4x4 LUT index
 * @y_rgb_cir_lut_idx: Y/RGB circular LUT index
 * @uv_cir_lut_idx:    UV circular LUT index
 * @y_rgb_sep_lut_idx: Y/RGB separable LUT index
 * @uv_sep_lut_idx:    UV separable LUT index
 * @de:                Detail enhancer settings
*/
struct sde_drm_scaler_v2 {
	/* General definitions*/
	uint32_t enable;
	uint32_t dir_en;

	/* Pix ext settings*/
	struct sde_drm_pix_ext_v1 pe;

	/* Decimation settings*/
	uint32_t horz_decimate;
	uint32_t vert_decimate;

	/* Phase settings*/
	int32_t init_phase_x[SDE_MAX_PLANES];
	int32_t phase_step_x[SDE_MAX_PLANES];
	int32_t init_phase_y[SDE_MAX_PLANES];
	int32_t phase_step_y[SDE_MAX_PLANES];

	uint32_t preload_x[SDE_MAX_PLANES];
	uint32_t preload_y[SDE_MAX_PLANES];
	uint32_t src_width[SDE_MAX_PLANES];
	uint32_t src_height[SDE_MAX_PLANES];

	uint32_t dst_width;
	uint32_t dst_height;

	uint32_t y_rgb_filter_cfg;
	uint32_t uv_filter_cfg;
	uint32_t alpha_filter_cfg;
	uint32_t blend_cfg;

	uint32_t lut_flag;
	uint32_t dir_lut_idx;

	/* for Y(RGB) and UV planes*/
	uint32_t y_rgb_cir_lut_idx;
	uint32_t uv_cir_lut_idx;
	uint32_t y_rgb_sep_lut_idx;
	uint32_t uv_sep_lut_idx;

	/* Detail enhancer settings */
	struct sde_drm_de_v1 de;
};

/* Scaler version definition, see top of file for guidelines */
#define SDE_DRM_SCALER_V1       0x1
#define SDE_DRM_SCALER_VERSION  SDE_DRM_SCALER_V1

/**
 * struct sde_drm_scaler - scaler structure
 * @version:    Structure version, set to SDE_DRM_SCALER_VERSION
 * @v1:         Version 1 of scaler structure
 */
struct sde_drm_scaler {
	uint64_t version;
	union {
		struct sde_drm_scaler_v1        v1;
	};
};

/*
 * Define constants for struct sde_drm_csc
 */
#define SDE_CSC_MATRIX_COEFF_SIZE   9
#define SDE_CSC_CLAMP_SIZE          6
#define SDE_CSC_BIAS_SIZE           3

/* CSC version definition, see top of file for guidelines */
#define SDE_DRM_CSC_V1              0x1
#define SDE_DRM_CSC_VERSION         SDE_DRM_CSC_V1

/**
 * struct sde_drm_csc_v1 - version 1 of struct sde_drm_csc
 * @ctm_coeff:          Matrix coefficients, in S31.32 format
 * @pre_bias:           Pre-bias array values
 * @post_bias:          Post-bias array values
 * @pre_clamp:          Pre-clamp array values
 * @post_clamp:         Post-clamp array values
 */
struct sde_drm_csc_v1 {
	int64_t ctm_coeff[SDE_CSC_MATRIX_COEFF_SIZE];
	uint32_t pre_bias[SDE_CSC_BIAS_SIZE];
	uint32_t post_bias[SDE_CSC_BIAS_SIZE];
	uint32_t pre_clamp[SDE_CSC_CLAMP_SIZE];
	uint32_t post_clamp[SDE_CSC_CLAMP_SIZE];
};

/**
 * struct sde_drm_csc - CSC configuration structure
 * @version: Structure version, set to SDE_DRM_CSC_VERSION
 * @v1:      Version 1 of csc structure
 */
struct sde_drm_csc {
	uint64_t version;
	union {
		struct sde_drm_csc_v1   v1;
	};
};

/* Writeback Config version definition */
#define SDE_DRM_WB_CFG		0x1

/* SDE_DRM_WB_CONFIG_FLAGS - Writeback configuration flags */
#define SDE_DRM_WB_CFG_FLAGS_CONNECTED	(1<<0)

/**
 * struct sde_drm_wb_cfg - Writeback configuration structure
 * @flags:		see DRM_MSM_WB_CONFIG_FLAGS
 * @connector_id:	writeback connector identifier
 * @count_modes:	Count of modes in modes_ptr
 * @modes:		Pointer to struct drm_mode_modeinfo
 */
struct sde_drm_wb_cfg {
	uint32_t flags;
	uint32_t connector_id;
	uint32_t count_modes;
	uint64_t modes;
};

#endif /* _SDE_DRM_H_ */
