#include "imu/ekf.hpp"
#include "core/util.hpp"
#include <iostream>

namespace vllm
{

Eigen::Matrix4f EKF::getState()
{
  Eigen::Matrix4f T = Eigen::Matrix4f::Identity();
  T.topLeftCorner(3, 3) = scale * qua.toRotationMatrix();
  T.topRightCorner(3, 1) = pos;
  return T;
}

Eigen::Quaternionf EKF::exp(const Eigen::Vector3f& v)
{
  float norm = v.norm();
  float c = std::cos(norm / 2);
  float s = std::sin(norm / 2);
  Eigen::Vector3f n = s * v.normalized();
  return Eigen::Quaternionf(c, n.x(), n.y(), n.z());
}
void EKF::init(const Eigen::Matrix4f& T, const Eigen::Vector3f& v)
{
  Eigen::Matrix3f R = T.topLeftCorner(3, 3);
  pos = T.topRightCorner(3, 1);
  qua = Eigen::Quaternionf(R).normalized();  // NOTE: must normalize here
  vel = v;

  P = 0.5 * Eigen::MatrixXf::Identity(9, 9);
}

void EKF::predict(const Eigen::Vector3f& acc, const Eigen::Vector3f& omega, unsigned long ns)
{
  if (!isUpadatable()) {
    last_ns = ns;
    return;
  }
  float dt = static_cast<float>(ns - last_ns) * 1e-9f;
  last_ns = ns;

  Eigen::Matrix3f R = qua.toRotationMatrix();
  Eigen::Quaternionf dq = exp(omega * dt);

  // Predict state
  Eigen::Vector3f nominal_acc = R * acc - gravity;
  pos += vel * dt + 0.5 * nominal_acc * dt * dt;
  vel += nominal_acc * dt;
  qua = qua * dq;

  // Propagate uncertainty
  Eigen::MatrixXf F = calcF(qua, acc, dt);
  P = F * P * F.transpose() + LQL * dt;

  // std::cout << "vel " << vel.transpose() << " n-acc " << nominal_acc.transpose() << " " << acc.transpose() << std::endl;
}

void EKF::observe(const Eigen::Matrix4f& T, unsigned long)
{
  // if (!isUpadatable()) {
  //   last_ns = ns;
  //   return;
  // }
  // float dt = static_cast<float>(ns - last_ns) * 1e-9f;
  // last_ns = ns;

  std::cout << "pre " << qua.x() << " " << qua.y() << " " << qua.z() << " " << qua.w() << " p " << pos.transpose() << std::endl;

  scale = getScale(T);
  Eigen::Matrix3f R = normalizeRotation(T);
  Eigen::Quaternionf q(R);
  Eigen::Vector3f t = T.topRightCorner(3, 1);
  std::cout << "obs-q " << q.x() << " " << q.y() << " " << q.z() << " " << q.w() << " p " << t.transpose() << std::endl;
  // observation jacobian  (7x9)
  Eigen::MatrixXf H = calcH(qua);
  // innovation covariance (7x7)
  Eigen::MatrixXf S = H * P * H.transpose() + W;
  Eigen::MatrixXf Si = S.inverse();  // NOTE: can use Cholesky decompose

  // kalman gain           (9x7)
  Eigen::MatrixXf K = P * H.transpose() * Si;
  // error vector          (7)
  Eigen::VectorXf error = toVec(t, q) - toVec(pos, qua);

  Eigen::VectorXf dx = K * error;
  Eigen::Quaternionf dq = exp(dx.bottomRows(3));

  // update
  pos += dx.topRows(3);
  vel += dx.block(3, 0, 3, 1);
  qua = qua * dq;
  P -= K * H * P;

  std::cout << "post-q " << qua.x() << " " << qua.y() << " " << qua.z() << " " << qua.w() << " p " << pos.transpose() << std::endl;
}

Eigen::MatrixXf EKF::calcH(const Eigen::Quaternionf& q)
{
  Eigen::MatrixXf Q(4, 3);
  // clang-format off
    Q << -q.x() , -q.y() , -q.z() ,
          q.w() , -q.z() ,  q.y() ,
          q.z() ,  q.w() , -q.x() ,
         -q.y() ,  q.x() ,  q.w() ;
  // clang-format on
  Q *= 0.5;

  Eigen::MatrixXf H = Eigen::MatrixXf::Zero(7, 9);
  H.topLeftCorner(3, 3).setIdentity();
  H.bottomRightCorner(4, 3) = Q;
  return H;
}

Eigen::MatrixXf EKF::calcF(const Eigen::Quaternionf& q, const Eigen::Vector3f& acc, float dt)
{
  Eigen::MatrixXf F = Eigen::MatrixXf::Identity(9, 9);
  F.block(0, 3, 3, 3) = Eigen::Matrix3f::Identity(3, 3) * dt;
  F.block(3, 6, 3, 3) = -hat(q.toRotationMatrix() * acc) * dt;
  return F;
}

Eigen::VectorXf EKF::toVec(const Eigen::Vector3f& p, const Eigen::Quaternionf& q)
{
  Eigen::VectorXf x(7);
  x.topRows(3) = p;
  x(3) = q.w();
  x(4) = q.x();
  x(5) = q.y();
  x(6) = q.z();
  return x;
}

Eigen::Matrix3f EKF::hat(const Eigen::Vector3f& vec)
{
  Eigen::Matrix3f A;
  // clang-format off
  A <<
        0, -vec(2),  vec(1),
   vec(2),       0, -vec(0),
  -vec(1),  vec(0),       0;
  // clang-format on
  return A;
}

}  // namespace vllm