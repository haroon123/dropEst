% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/quality_control.R
\name{GetUmisDistribution}
\alias{GetUmisDistribution}
\title{Plot distribution of UMI}
\usage{
GetUmisDistribution(reads.per.umi.per.cb, smooth = 1, trim.quantile = 0.99,
  bins = 50)
}
\arguments{
\item{bins}{number of bins in histogram}
}
\value{
ggplot object with the plot.
}
\description{
Plot distribution of UMI
}
