#pragma once

#include "webtiles/common.h"
#include "webtiles/fbs/sparse_block_generated.h"

namespace webtiles::index {

fbs::SparseBlockT denseToSparse(const fbs::SparseBlockT& block);
fbs::SparseBlockT sparseToDense(const fbs::SparseBlockT& block);

fbs::Location queryBlock(const fbs::SparseBlock* block, TileId tileId);

} // namespace webtiles::index
