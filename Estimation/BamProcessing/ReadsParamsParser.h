#pragma once

#include <api/BamAlignment.h>
#include <Tools/RefGenesContainer.h>

namespace Tools
{
	class ReadParameters;
}

namespace Estimation
{
	namespace BamProcessing
	{
		class ReadsParamsParser
		{
		public:
			enum GeneMatchLevel
			{
				ANY,
				ONE,
				BOTH,
				SIZE
			};
		private:
			Tools::RefGenesContainer _genes_container;
			int _gene_match_level;

		public:
			ReadsParamsParser(const std::string &genes_filename, int gene_match_level);

			virtual bool get_read_params(const BamTools::BamAlignment &alignment, Tools::ReadParameters &read_params);
			std::string get_gene(const std::string &chr_name, BamTools::BamAlignment alignment) const;
		};
	}
}
