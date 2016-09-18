#pragma once

#include <RInside.h>
#include "RealBarcodesMergeStrategy.h"

namespace TestEstimatorMergeProbs
{
	struct testPoissonMergeProbs;
	struct testPoissonMergeInit;
	struct testPoissonMergeTime;
}

namespace Estimation
{
namespace Merge
{
	class BrokenRealBarcodesMergeStrategy : public RealBarcodesMergeStrategy
	{
	protected:
		virtual void init(const Estimation::CellsDataContainer &container) override;

		virtual long get_best_merge_target(const CellsDataContainer &container, size_t base_cell_ind, const ul_list_t &neighbour_cells) const override;
		virtual long get_max_merge_dist(long min_real_cb_dist) const override;

	public:
		BrokenRealBarcodesMergeStrategy(const std::string &barcodes_filename, size_t barcode2_length,
										 int min_genes_before_merge, int min_genes_after_merge,
										 int max_merge_edit_distance, double min_merge_fraction);

		virtual std::string merge_type() const override;
	};
}
}