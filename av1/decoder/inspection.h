#ifndef AOM_INSPECTION_H_
#define AOM_INSPECTION_H_

#if CONFIG_ACCOUNTING
#include "av1/common/accounting.h"
#endif

typedef void (*aom_inspect_cb)(void *decoder, void *data);

typedef struct insp_mv insp_mv;

struct insp_mv {
  int16_t row;
  int16_t col;
};

typedef struct insp_mi_data insp_mi_data;

struct insp_mi_data {
  insp_mv mv[2];
  int8_t ref_frame[2];
  int8_t mode;
  int8_t sb_type;
  int8_t skip;
  int8_t segment_id;
  int8_t filter;
  int8_t tx_type;
  int8_t tx_size;
#if CONFIG_CDEF
// TODO(negge): add per block CDEF data
#endif
};

typedef struct insp_frame_data insp_frame_data;

struct insp_frame_data {
#if CONFIG_ACCOUNTING
  Accounting *accounting;
#endif
  insp_mi_data *mi_grid;
  int show_frame;
  int frame_type;
  int base_qindex;
  int mi_rows;
  int mi_cols;
  int16_t y_dequant[MAX_SEGMENTS][2];
  int16_t uv_dequant[MAX_SEGMENTS][2];
#if CONFIG_CDEF
// TODO(negge): add per frame CDEF data
#endif
};

void ifd_init(insp_frame_data *fd, int frame_width, int frame_height);
void ifd_clear(insp_frame_data *fd);
int ifd_inspect(insp_frame_data *fd, void *decoder);

#endif
