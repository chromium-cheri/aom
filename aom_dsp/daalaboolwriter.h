#ifndef AOM_DSP_DAALABOOLWRITER_H_
#define AOM_DSP_DAALABOOLWRITER_H_

#include "aom_dsp/entenc.h"
#include "aom_dsp/prob.h"

#ifdef __cplusplus
extern "C" {
#endif

struct daala_writer {
  unsigned int pos;
  uint8_t *buffer;
  od_ec_enc ec;
};

typedef struct daala_writer daala_writer;

void aom_daala_start_encode(daala_writer *bc, uint8_t *buffer);
void aom_daala_stop_encode(daala_writer *bc);

static INLINE void aom_daala_write(daala_writer *br, int bit, int probability) {
  if (probability == 128) {
    od_ec_enc_bits(&br->ec, bit, 1);
  }
  else {
    int p = (32768*probability + (256 - probability)) >> 8;
    od_ec_encode_bool_q15(&br->ec, bit, p);
  }
}

static INLINE void daala_write_tree_bits(daala_writer *w,
                                         const aom_tree_index *tree,
                                         const aom_prob *probs, int bits,
                                         int len, aom_tree_index i) {
  aom_tree_index root;
  root = i;
  do {
    uint16_t cdf[16];
    aom_tree_index index[16];
    int path[16];
    int dist[16];
    int nsymbs;
    int symb;
    int j;
    /* Compute the CDF of the binary tree using the given probabilities. */
    nsymbs = tree_to_cdf(tree, probs, root, cdf, index, path, dist);
    /* Find the symbol to code. */
    symb = -1;
    for (j = 0; j < nsymbs; j++) {
      /* If this symbol codes a leaf node,  */
      if (index[j] <= 0) {
        if (len == dist[j] && path[j] == bits) {
          symb = j;
          break;
        }
      }
      else {
        if (len > dist[j] && path[j] == bits >> (len - dist[j])) {
          symb = j;
          break;
        }
      }
    }
    OD_ASSERT(symb != -1);
    od_ec_encode_cdf_q15(&w->ec, symb, cdf, nsymbs);
    bits &= (1 << (len - dist[symb])) - 1;
    len -= dist[symb];
  }
  while (len);
}

static INLINE void daala_write_tree_cdf(daala_writer *w, int symb,
                                        const uint16_t *cdf, int nsymbs) {
  od_ec_encode_cdf_q15(&w->ec, symb, cdf, nsymbs);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
