#ifndef LINEBUFFER_H
#define LINEBUFFER_H

#include "Stencil.h"

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <hls_stream.h>

using hls::stream;


template <size_t N, typename T,
          size_t EXTENT_0, size_t EXTENT_1=1, size_t EXTENT_2=1, size_t EXTENT_3=1>
struct PackedStencilStreamGroup {
    stream<PackedStencil<T, EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> > *val[N];

    PackedStencil<T, EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> read(size_t index) {
#pragma HLS INLINE
        return val[index]->read();
    }
};

template <size_t IMG_EXTENT_0, size_t EXTENT_1, size_t EXTENT_2, size_t EXTENT_3,
	  size_t IN_EXTENT_0,  size_t OUT_EXTENT_0, typename T, size_t N>
void linebuffer_1D(stream<PackedStencil<T, IN_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> > &in_stream,
                   PackedStencilStreamGroup<N, T, OUT_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> out_streams) {
    static_assert(IMG_EXTENT_0 > OUT_EXTENT_0, "image extent not is larger than output.");
    static_assert(OUT_EXTENT_0 > IN_EXTENT_0, "input extent is larger than output."); // TODO handle this situation.
    static_assert(IMG_EXTENT_0 % IN_EXTENT_0 == 0, "image extent is not divisible by input."); // TODO handle this situation.
    static_assert(OUT_EXTENT_0 % IN_EXTENT_0 == 0, "output extent is not divisible by input."); // TODO handle this situation.
#pragma HLS INLINE

    const size_t buffer_size = OUT_EXTENT_0 / IN_EXTENT_0;
    PackedStencil<T, IN_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> buffer[buffer_size];  // shift register
#pragma HLS ARRAY_PARTITION variable=buffer complete dim=1

    // initialize buffer
 LB_1D_init:for (size_t i = 0; i < buffer_size - 1; i++) {
#pragma HLS PIPELINE
        PackedStencil<T, IN_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> in_stencil = in_stream.read();
        buffer[i] = in_stencil;
    }

    // produce one output per input
    const size_t NUM_OF_OUTPUT_0 = (IMG_EXTENT_0 - OUT_EXTENT_0) / IN_EXTENT_0 + 1;
 LB_1D_shift:for(size_t n = 0; n < NUM_OF_OUTPUT_0; n++) {
#pragma HLS PIPELINE
        PackedStencil<T, IN_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> in_stencil = in_stream.read();
        PackedStencil<T, OUT_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> out_stencil;

        buffer[buffer_size - 1] = in_stencil;

        // convert buffer to out_stencil, doing bit shuffling essentially
        for(size_t idx_3 = 0; idx_3 < EXTENT_3; idx_3++)
#pragma HLS UNROLL
        for(size_t idx_2 = 0; idx_2 < EXTENT_2; idx_2++)
#pragma HLS UNROLL
        for(size_t idx_1 = 0; idx_1 < EXTENT_1; idx_1++)
#pragma HLS UNROLL
        for(size_t idx_0 = 0; idx_0 < IN_EXTENT_0; idx_0++)
#pragma HLS UNROLL
        for(size_t idx_buffer = 0; idx_buffer < buffer_size; idx_buffer++) {
#pragma HLS UNROLL
            out_stencil(idx_0+idx_buffer*IN_EXTENT_0, idx_1, idx_2, idx_3)
                = buffer[idx_buffer](idx_0, idx_1, idx_2, idx_3);
        }

        for(size_t i = 0; i < N; i++) {
#pragma HLS UNROLL
            out_streams.val[i]->write(out_stencil);
        }

        // shift
        for (size_t i = 0; i < buffer_size - 1; i++)
            buffer[i] = buffer[i+1];
    }
}

// An overloaded (trivial) 1D line buffer, where all the input dimensions and
// the output dimensions are the same size
template <size_t IMG_EXTENT_0,
          size_t EXTENT_0, size_t EXTENT_1, size_t EXTENT_2, size_t EXTENT_3, typename T, size_t N>
void linebuffer_1D(stream<PackedStencil<T, EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> > &in_stream,
                   PackedStencilStreamGroup<N, T, EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> out_streams) {
#pragma HLS INLINE
 LB_1D_pass:for(size_t idx_0 = 0; idx_0 < IMG_EXTENT_0; idx_0 += EXTENT_0) {
#pragma HLS PIPELINE
        PackedStencil<T, EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> out_stencil = in_stream.read();

        for(size_t i = 0; i < N; i++) {
#pragma HLS UNROLL
            out_streams.val[i]->write(out_stencil);
        }

    }
}




template <size_t IMG_EXTENT_0, size_t IMG_EXTENT_1, size_t EXTENT_2, size_t EXTENT_3,
	  size_t IN_EXTENT_0, size_t IN_EXTENT_1,
	  size_t OUT_EXTENT_1, typename T>
void linebuffer_2D_col(stream<PackedStencil<T, IN_EXTENT_0, IN_EXTENT_1, EXTENT_2, EXTENT_3> > &in_stream,
		       stream<PackedStencil<T, IN_EXTENT_0, OUT_EXTENT_1, EXTENT_2, EXTENT_3> > &out_stream) {
    static_assert(OUT_EXTENT_1 % IN_EXTENT_1 == 0, "output extent is not divisible by input."); // TODO handle this situation.
#pragma HLS INLINE off

    const size_t NUM_OF_LINES = OUT_EXTENT_1 / IN_EXTENT_1;
    const size_t NUM_OF_COLS = IMG_EXTENT_0 / IN_EXTENT_0;
    //Stencil<T, IMG_EXTENT_0, OUT_EXTENT_1, EXTENT_2, EXTENT_3> linebuffer;
    PackedStencil<T, IN_EXTENT_0, IN_EXTENT_1, EXTENT_2, EXTENT_3> linebuffer[NUM_OF_LINES][NUM_OF_COLS];
#pragma HLS ARRAY_PARTITION variable=linebuffer complete dim=1

    // initialize linebuffer, i.e. fill first (OUT_EXTENT_1 - IN_EXTENT_1) lines
    size_t write_idx_1 = 0;  // the line index of coming stencil in the linebuffer
 LB_2D_col_init:for (; write_idx_1 < NUM_OF_LINES - 1; write_idx_1++) {
        for (size_t idx_col = 0; idx_col < NUM_OF_COLS; idx_col++) {
#pragma HLS PIPELINE
            linebuffer[write_idx_1][idx_col] = in_stream.read();
        }
    }

    // produce a [IN_EXTENT_0, OUT_EXTENT_1] column stencil per input
    const size_t NUM_OF_OUTPUT_1 = (IMG_EXTENT_1 - OUT_EXTENT_1) / IN_EXTENT_1 + 1;
 LB_2D_col:for (size_t n1 = 0; n1 < NUM_OF_OUTPUT_1; n1++) {
#pragma HLS loop_flatten off
	if (write_idx_1 >= NUM_OF_LINES)
	    write_idx_1 -= NUM_OF_LINES;

	//for(size_t idx_0 = 0; idx_0 < IMG_EXTENT_0; idx_0 += IN_EXTENT_0)  {
        for (size_t idx_col = 0; idx_col < NUM_OF_COLS; idx_col++) {
#pragma HLS PIPELINE
            linebuffer[write_idx_1][idx_col] = in_stream.read();

	    PackedStencil<T, IN_EXTENT_0, OUT_EXTENT_1, EXTENT_2, EXTENT_3> col_buf;

            for(size_t idx_line = 0; idx_line < NUM_OF_LINES; idx_line++) {
#pragma HLS UNROLL
                size_t idx_line_in_buffer = idx_line + write_idx_1 + 1;
                if (idx_line_in_buffer >= NUM_OF_LINES)
                    idx_line_in_buffer -= NUM_OF_LINES;

            for(size_t idx_3 = 0; idx_3 < EXTENT_3; idx_3++)
#pragma HLS UNROLL
            for(size_t idx_2 = 0; idx_2 < EXTENT_2; idx_2++)
#pragma HLS UNROLL
            for(size_t idx_1 = 0; idx_1 < IN_EXTENT_1; idx_1++)
#pragma HLS UNROLL
            for(size_t idx_0 = 0; idx_0 < IN_EXTENT_0; idx_0++)
#pragma HLS UNROLL
                col_buf(idx_0, idx_line*IN_EXTENT_0 + idx_1, idx_2, idx_3)
                    = linebuffer[idx_line_in_buffer][idx_col](idx_0, idx_1, idx_2, idx_3);
            }

	    out_stream.write(col_buf);
	}
	//idx_1 += IN_EXTENT_1;  // increase the line address
        write_idx_1++;
    }
}


template <size_t IMG_EXTENT_0, size_t IMG_EXTENT_1, size_t EXTENT_2, size_t EXTENT_3,
	  size_t IN_EXTENT_0, size_t IN_EXTENT_1,
	  size_t OUT_EXTENT_0, size_t OUT_EXTENT_1, typename T, size_t N>
void linebuffer_2D(stream<PackedStencil<T, IN_EXTENT_0, IN_EXTENT_1, EXTENT_2, EXTENT_3> > &in_stream,
                   PackedStencilStreamGroup<N, T, OUT_EXTENT_0, OUT_EXTENT_1, EXTENT_2, EXTENT_3> out_streams) {
    static_assert(IMG_EXTENT_1 > OUT_EXTENT_1, "output extent is larger than image.");
    static_assert(OUT_EXTENT_1 > IN_EXTENT_1, "input extent is larger than output."); // TODO handle this situation.
    static_assert(IMG_EXTENT_1 % IN_EXTENT_1 == 0, "image extent is not divisible by input."); // TODO handle this situation.
    static_assert(OUT_EXTENT_1 % IN_EXTENT_1 == 0, "output extent is not divisible by input."); // TODO handle this situation.
#pragma HLS INLINE

    stream<PackedStencil<T, IN_EXTENT_0, OUT_EXTENT_1, EXTENT_2, EXTENT_3> > col_buf_stream;
#pragma HLS STREAM variable=col_buf_stream depth=1
#pragma HLS RESOURCE variable=col_buf_stream core=FIFO_SRL

    // use a 2D storage to buffer lines of image,
    // and output a column stencil per input at steady state
    linebuffer_2D_col<IMG_EXTENT_0, IMG_EXTENT_1>(in_stream, col_buf_stream);

    // feed the column stencil stream to 1D line buffer
    const size_t NUM_OF_OUTPUT_1 = (IMG_EXTENT_1 - OUT_EXTENT_1) / IN_EXTENT_1 + 1;
 LB_2D_shift_reg:for (size_t n1 = 0; n1 < NUM_OF_OUTPUT_1; n1++) {
#pragma HLS loop_flatten off
	linebuffer_1D<IMG_EXTENT_0>(col_buf_stream, out_streams);
    }
}

// An overloaded (trivial) 2D line buffer, where input dim 1 and output dim 1 are the same size
template <size_t IMG_EXTENT_0, size_t IMG_EXTENT_1,
          size_t IN_EXTENT_0, size_t OUT_EXTENT_0,
          size_t EXTENT_1, size_t EXTENT_2, size_t EXTENT_3, typename T, size_t N>
void linebuffer_2D(stream<PackedStencil<T, IN_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> > &in_stream,
                   PackedStencilStreamGroup<N, T, OUT_EXTENT_0, EXTENT_1, EXTENT_2, EXTENT_3> out_streams) {
#pragma HLS INLINE
 LB_2D_pass:for(size_t idx_1 = 0; idx_1 < IMG_EXTENT_1; idx_1 += EXTENT_1) {
	linebuffer_1D<IMG_EXTENT_0>(in_stream, out_streams);
    }
}


/** A line buffer that buffers a image size [IMG_EXTENT_0, IMG_EXTENT_1, IMG_EXTENT_2].
 * The input is a stencil size [IN_EXTENT_0, IN_EXTENT_1, IN_EXTENT_2], and it traversal
 * the image along dimensiO 0 first, and then dimension 1, and so on. The step of the
 * input stencil is the same as the size of input stencil, so there is no overlapping
 * between input stencils.
 * The output is a stencil size [OUT_EXTENT_0, OUT_EXTENT_1, OUT_EXTENT_2], and it traversal
 * the image the same as input (i.e. along dimension 0 first, and then dimension 1, and so on.
 * The step of the output stencil is the same as the size of input stencil, so the
 * throughputs of the inputs and outputs are balanced at the steady state. In other words,
 * the line buffer generates one output per input at the steady state.
 */
template <size_t IMG_EXTENT_0, size_t IMG_EXTENT_1=1, size_t IMG_EXTENT_2=1, size_t IMG_EXTENT_3=1,
	  size_t IN_EXTENT_0, size_t IN_EXTENT_1, size_t IN_EXTENT_2, size_t IN_EXTENT_3,
	  size_t OUT_EXTENT_0, size_t OUT_EXTENT_1, size_t OUT_EXTENT_2, size_t OUT_EXTENT_3,
	  typename T, size_t N>
void linebuffer(stream<PackedStencil<T, IN_EXTENT_0, IN_EXTENT_1, IN_EXTENT_2, IN_EXTENT_3> > &in_stream,
		PackedStencilStreamGroup<N, T, OUT_EXTENT_0, OUT_EXTENT_1, OUT_EXTENT_2, OUT_EXTENT_3> out_streams) {
    static_assert(IMG_EXTENT_3 == IN_EXTENT_3 && IMG_EXTENT_3 == OUT_EXTENT_3,
		  "dont not support 4D line buffer yet.");
    static_assert(IMG_EXTENT_2 == IN_EXTENT_2 && IMG_EXTENT_2 == OUT_EXTENT_2,
		  "dont not support 3D line buffer yet.");
#pragma HLS INLINE
    linebuffer_2D<IMG_EXTENT_0, IMG_EXTENT_1>(in_stream, out_streams);
}


template <size_t IMG_EXTENT_0, size_t IMG_EXTENT_1=1, size_t IMG_EXTENT_2=1, size_t IMG_EXTENT_3=1,
	  size_t IN_EXTENT_0, size_t IN_EXTENT_1, size_t IN_EXTENT_2, size_t IN_EXTENT_3,
          size_t OUT_EXTENT_0, size_t OUT_EXTENT_1, size_t OUT_EXTENT_2, size_t OUT_EXTENT_3,
          typename T>
void linebuffer_ref(stream<PackedStencil<T, IN_EXTENT_0, IN_EXTENT_1, IN_EXTENT_2, IN_EXTENT_3> > &in_stream,
		    stream<PackedStencil<T, OUT_EXTENT_0, OUT_EXTENT_1, OUT_EXTENT_2, OUT_EXTENT_3> > &out_stream) {

    T buffer[IMG_EXTENT_3][IMG_EXTENT_2][IMG_EXTENT_1][IMG_EXTENT_0];

    for(size_t outer_3 = 0; outer_3 < IMG_EXTENT_3; outer_3 += IN_EXTENT_3)
    for(size_t outer_2 = 0; outer_2 < IMG_EXTENT_2; outer_2 += IN_EXTENT_2)
    for(size_t outer_1 = 0; outer_1 < IMG_EXTENT_1; outer_1 += IN_EXTENT_1)
    for(size_t outer_0 = 0; outer_0 < IMG_EXTENT_0; outer_0 += IN_EXTENT_0) {
        Stencil<T, IN_EXTENT_0, IN_EXTENT_1, IN_EXTENT_2, IN_EXTENT_3> stencil = in_stream.read();

        for(size_t inner_3 = 0; inner_3 < IN_EXTENT_3; inner_3++)
        for(size_t inner_2 = 0; inner_2 < IN_EXTENT_2; inner_2++)
        for(size_t inner_1 = 0; inner_1 < IN_EXTENT_1; inner_1++)
        for(size_t inner_0 = 0; inner_0 < IN_EXTENT_0; inner_0++)
            buffer[outer_3+inner_3][outer_2+inner_2][outer_1+inner_1][outer_0+inner_0]
                = stencil(inner_0, inner_1, inner_2, inner_3);
    }

    for(size_t outer_3 = 0; outer_3 <= IMG_EXTENT_3 - OUT_EXTENT_3; outer_3 += IN_EXTENT_3)
    for(size_t outer_2 = 0; outer_2 <= IMG_EXTENT_2 - OUT_EXTENT_2; outer_2 += IN_EXTENT_2)
    for(size_t outer_1 = 0; outer_1 <= IMG_EXTENT_1 - OUT_EXTENT_1; outer_1 += IN_EXTENT_1)
    for(size_t outer_0 = 0; outer_0 <= IMG_EXTENT_0 - OUT_EXTENT_0; outer_0 += IN_EXTENT_0) {
        Stencil<T, OUT_EXTENT_0, OUT_EXTENT_1, OUT_EXTENT_2, OUT_EXTENT_3> stencil;

        for(size_t inner_3 = 0; inner_3 < OUT_EXTENT_3; inner_3++)
        for(size_t inner_2 = 0; inner_2 < OUT_EXTENT_2; inner_2++)
        for(size_t inner_1 = 0; inner_1 < OUT_EXTENT_1; inner_1++)
        for(size_t inner_0 = 0; inner_0 < OUT_EXTENT_0; inner_0++)
            stencil(inner_0, inner_1, inner_2, inner_3)
                = buffer[outer_3+inner_3][outer_2+inner_2][outer_1+inner_1][outer_0+inner_0];

		out_stream.write(stencil);
    }
}


#endif
