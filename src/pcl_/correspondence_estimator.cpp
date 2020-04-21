/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2012-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id$
 *
 */
#ifndef PCL_REGISTRATION_IMPL_CORRESPONDENCE_ESTIMATION_BACK_PROJECTION_HPP_
#define PCL_REGISTRATION_IMPL_CORRESPONDENCE_ESTIMATION_BACK_PROJECTION_HPP_

#include "vllm/pcl_/correspondence_estimator.hpp"
#include <pcl/common/copy_point.h>

namespace vllm
{
namespace pcl_
{
///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointSource, typename PointTarget, typename NormalT, typename Scalar>
bool CorrespondenceEstimationBackProjection<PointSource, PointTarget, NormalT, Scalar>::initCompute()
{
  if (!source_normals_ || !target_normals_) {
    PCL_WARN("[pcl::registration::%s::initCompute] Datasets containing normals for source/target have not been given!\n", getClassName().c_str());
    return (false);
  }

  return (pcl::registration::CorrespondenceEstimationBase<PointSource, PointTarget, Scalar>::initCompute());
}

///////////////////////////////////////////////////////////////////////////////////////////
template <typename PointSource, typename PointTarget, typename NormalT, typename Scalar>
void CorrespondenceEstimationBackProjection<PointSource, PointTarget, NormalT, Scalar>::determineCorrespondences(
    pcl::Correspondences& correspondences, double max_distance)
{
  if (!initCompute())
    return;

  correspondences.resize(indices_->size());

  std::vector<int> nn_indices(k_);
  std::vector<float> nn_dists(k_);

  float min_dist = std::numeric_limits<float>::max();
  int min_index = 0;
  float min_output_dist = 0;

  pcl::Correspondence corr;
  unsigned int nr_valid_correspondences = 0;

  constexpr float gain_center[] = {-0.2f, 0.0f, 0.2f};
  constexpr int gain_K[] = {1, 2, 3};

  // Check if the template types are the same. If true, avoid a copy.
  // Both point types MUST be registered using the POINT_CLOUD_REGISTER_POINT_STRUCT macro!
  if (pcl::isSamePointType<PointSource, PointTarget>()) {

    PointTarget pt;
    // Iterate over the input set of source indices
    for (std::vector<int>::const_iterator idx_i = indices_->begin(); idx_i != indices_->end(); ++idx_i) {

      Eigen::Vector3f distance_from_center = (input_->points[*idx_i].getVector3fMap() - center_).normalized();

      min_dist = std::numeric_limits<float>::max();

      for (int k = 0; k < 3; k++) {
        Eigen::Vector3f offset_center = input_->points[*idx_i].getVector3fMap() + gain_center[k] * distance_from_center;
        tree_->nearestKSearch(PointSource(offset_center.x(), offset_center.y(), offset_center.z()), k_ * gain_K[k], nn_indices, nn_dists);

        // Among the K nearest neighbours find the one with minimum perpendicular distance to the normal
        float tmp_min_dist = std::numeric_limits<float>::max();
        int tmp_min_index = 0;
        float tmp_output_dist = 0;

        // Find the best correspondence
        for (size_t j = 0; j < nn_indices.size(); j++) {
          float cos_angle = source_normals_->points[*idx_i].normal_x * target_normals_->points[nn_indices[j]].normal_x + source_normals_->points[*idx_i].normal_y * target_normals_->points[nn_indices[j]].normal_y + source_normals_->points[*idx_i].normal_z * target_normals_->points[nn_indices[j]].normal_z;
          float dist = nn_dists[j] * (2.0f - cos_angle * cos_angle);

          if (dist < tmp_min_dist) {
            tmp_min_dist = dist;
            tmp_min_index = nn_indices[j];
            tmp_output_dist = nn_dists[j];
          }
        }

        if (tmp_min_dist < min_dist) {
          min_dist = tmp_min_dist;
          min_index = tmp_min_index;
          min_output_dist = tmp_output_dist;
        }
      }
      if (min_dist > max_distance)
        continue;

      corr.index_query = *idx_i;
      corr.index_match = min_index;
      corr.distance = min_output_dist;
      correspondences[nr_valid_correspondences++] = corr;
    }
  } else {
    PCL_WARN("called the NOT implemented function in CorrespondenceEstimationBackprojection::determinCorrespondence!\n", getClassName().c_str());
  }
  correspondences.resize(nr_valid_correspondences);
  deinitCompute();
}
template class CorrespondenceEstimationBackProjection<pcl::PointXYZ, pcl::PointXYZ, pcl::Normal>;

}  // namespace pcl_
}  // namespace vllm

#endif  // PCL_REGISTRATION_IMPL_CORRESPONDENCE_ESTIMATION_BACK_PROJECTION_HPP_