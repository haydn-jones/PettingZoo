#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

#include <stdint.h>

#define IN_H 4
#define IN_W 4
#define OUTPUT_SIZE 3
#define HLC 3
#define NPL 16

/*

All matrices in the chromosome will be flattened, 2D, row-major matrices with elements of type
UINT_8 unless otherwise stated.

The parameters that define the size of a chromosome (notably nodes per hidden layer and hidden
layer count) will hopefully change so we will embed the size parameters in the beginning of a
chcromosome to allow some amount of portability. The first elements of a chromosome will be as
follows (byte indexed):

    0: IN_W (input width)
    1: IN_H (input height)
    2-3: NPL (nodes per hidden layer)
    4: HLC (hidden layer count)

The next chunk of the chromosome will be a matrix of size (IN_H, IN_W) describing which
input tiles are active. It currently makes sense to me that an inactive input tile should
report that there is simply nothing there (empty tile). A 0 in this matrix at index i,j means
that input tile i,j is inactive. A 1 means the input is active.

The next chunk will be a matrix of size (HLC, NPL) describing which neurons are active in the
hidden layers. A 0 means active, and a 1 means inactive.

The next chunk will be a matrix of size (IN_H * IN_W, NPL) and is an adjacency matrix
describing the weights connecting the input nodes to the first hidden layer. Each element will
be of type float.

The next (HLC - 1) chunks will be adjacency matrices of size (NPL, NPL) describing weights
between the previous hidden layer and the current. Each element will be of type float.

The final chunk will be an adjacency matrix of size (NPL, OUTPUT_SIZE) describing the weights
between the final hidden layer and the output layer. Each element will be of type float.

*/

#endif