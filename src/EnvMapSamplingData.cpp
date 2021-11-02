#include <EnvMapSamplingData.h>

#include <algorithm>
#include <numeric>
#include <vector>

EnvMapSamplingData::EnvMapSamplingData(vsg::ref_ptr<vsg::vec4Array2D> envMap)
{
  uint32_t height = envMap->height();
  uint32_t width = envMap->width();

  pdf = generatePDF(envMap);

  std::vector<std::vector<float>> conditionalCDFVec;
  std::vector<float> marginalPDF(height);
  for (uint32_t i = 0; i < height; ++i) {
    // Calculate PDF p(u) of the marginal distribution of vertical position u
    // p(u) = çp(u,v)dv
    marginalPDF[i] = 0.0f; 
    for (uint32_t j = 0; j < width; ++j) {
      // vsg::Array2D#at function uses at(column, row) order
      marginalPDF[i] += pdf->at(j, i);
    }

    // Calculate PDF p(u|v) for conditional distribution of the i-th row
    std::vector<float> conditionalPDF(width);
    for (uint32_t j = 0; j < width; ++j) {
      conditionalPDF[j] = pdf->at(j, i) / marginalPDF[i];  // p(v|u) = p(u,v) / p(u)
    }

    assert(approxEq(std::accumulate(conditionalPDF.begin(), conditionalPDF.end(), 0.0f), 1.0f, 1.0e-3f));  // Sum of a PDF must be 1

    // Generate CDF for conditional distributions of each row
    conditionalCDFVec.push_back(generateCDF1D(conditionalPDF));
  }

  // Copy CDF of conditional distributions into a vsg::float2DArray
  conditionalCDF = vsg::floatArray2D::create(width, height);
  for (uint32_t i = 0; i < height; ++i) {
    for (uint32_t j = 0; j < width; ++j) {
      conditionalCDF->at(j, i) = conditionalCDFVec[i][j];
    }
  }

  assert(approxEq(std::accumulate(marginalPDF.begin(), marginalPDF.end(), 0.0f), 1.0f, 1.0e-2f));  // Sum of a PDF must be 1

  // Generate CDF for the marginal distribution
  std::vector<float> marginalCDFVec = generateCDF1D(marginalPDF);
  // Copy CDF of the marginal distribution into a vsg::floatArray
  marginalCDF = vsg::floatArray::create(height);
  std::copy(marginalCDFVec.begin(), marginalCDFVec.end(), marginalCDF->begin());
}

vsg::ref_ptr<vsg::floatArray2D> EnvMapSamplingData::generatePDF(vsg::ref_ptr<vsg::vec4Array2D> envMap)
{
  uint32_t height = envMap->height();
  uint32_t width = envMap->width();

  auto pdf = vsg::floatArray2D::create(width, height);

  // Calculate relative luminance of each pixels
  float luminanceSum = 0.0;  // Sum of luminance for normalization
  for (uint32_t i = 0; i < height; ++i) {
    for (uint32_t j = 0; j < width; ++j) {
      // vsg::Array2D#at function uses at(column, row) order
      const vsg::vec4& color = envMap->at(j, i);
      // Calculate relative luminance from linear RGB
      // See: https://en.wikipedia.org/w/index.php?title=Relative_luminance&oldid=1051312528
      pdf->at(j, i) = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
      luminanceSum += pdf->at(j, i);
    }
  }

  // Normalize relative luminance to make probability distribution function (PDF)
  for (uint32_t i = 0; i < height; ++i) {
    for (uint32_t j = 0; j < width; ++j) {
      pdf->at(j, i) /= luminanceSum;
    }
  }

  // Sum of a PDF must be 1
  assert(approxEq(std::accumulate(pdf->begin(), pdf->end(), 0.0f), 1.0f, 1.0e-3f));

  return pdf;
}

std::vector<float> EnvMapSamplingData::generateCDF1D(const std::vector<float>& pdf)
{
  std::vector<float> cdf(pdf.size());
  float prev = 0.0f;
  for (size_t i = 0; i < pdf.size(); ++i) {
    cdf[i] = prev + pdf[i];
    prev = cdf[i];
  }

  assert(approxEq(cdf[cdf.size() - 1], 1.0f, 1.0e-2f)); // Last element of a CDF array must be 1

  return cdf;
}
