#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.7;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.5;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  is_initialized_ = false;
  n_x_ = 5;
  n_aug_ = 7;
  lambda_ = 3 - n_aug_;
  weights_ = VectorXd(2 * n_aug_ + 1);
  weights_(0) = lambda_ / (lambda_ + n_aug_);
  for (auto i = 1; i != 2 * n_aug_ + 1; i++) {
    weights_(i) = 0.5 / (n_aug_ + lambda_);
  }
  P_ = MatrixXd().Identity(n_x_, n_x_);
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */

  // 1. Initialize
  if (!is_initialized_) {
    if (use_laser_ && meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      float ro = meas_package.raw_measurements_[0];
      float theta = meas_package.raw_measurements_[1];
      float ro_dot = meas_package.raw_measurements_[2];
      float px = ro * cos(theta);
      float py = ro * sin(theta);
      x_ << px, py, 0, 0, 0;
      is_initialized_ = true;
    }
    
    if (use_radar_ && meas_package.sensor_type_ == MeasurementPackage::LASER) {
      float px = meas_package.raw_measurements_[0];
      float py = meas_package.raw_measurements_[1];
      x_ << px, py, 0, 0, 0;
      is_initialized_ = true;
    }

    time_us_ = meas_package.timestamp_;  
    return;
  }

  double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;

  // 2. Predict
  Prediction(delta_t);
  
  // 3. Update
  if (use_laser_ && meas_package.sensor_type_ == MeasurementPackage::LASER) {
    UpdateLidar(meas_package);
  }
  else if (use_radar_ && meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    UpdateRadar(meas_package);
  }

  cout << "x: " << x_ << endl;
  cout << "P: " << P_ << endl;
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */

  /** 
  */

  /** Step1: UKF Augmentation
  */
  // 1.1. create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);

  x_aug.head(n_x_) = x_;
  for (auto i = n_x_; i != n_aug_; i++) {
    x_aug(i) = 0;
  }

  // 1.2. create augmented state covariance
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  
  P_aug.fill(0);
  auto m = n_aug_ - n_x_;
  MatrixXd Q = MatrixXd(m, m);
  Q << std_a_ * std_a_, 0,
    0, std_yawdd_ * std_yawdd_;

  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug.bottomRightCorner(m, m) = Q;

  // 1.3. create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  
  MatrixXd A = P_aug.llt().matrixL();
  Xsig_aug.col(0) = x_aug;
  for (auto i = 0; i != n_aug_; i++) {
    Xsig_aug.col(i + 1) = x_aug + sqrt(lambda_ + n_aug_) * A.col(i);
    Xsig_aug.col(i + n_aug_ + 1) = x_aug - sqrt(lambda_ + n_aug_) * A.col(i);
  }

  /** Step2: Sigma Point Prediction
  */
  //create matrix with predicted sigma points as columns
  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    double px = Xsig_aug(0, i);
    double py = Xsig_aug(1, i);
    double v = Xsig_aug(2, i);
    double yaw = Xsig_aug(3, i);
    double yawd = Xsig_aug(4, i);
    double nu_a = Xsig_aug(5, i);
    double nu_yawdd = Xsig_aug(6, i);

    double px_p = 0.0, py_p = 0.0;
    //avoid division by zero
    if (fabs(yawd) > 0.00001) {
      px_p = px + v * (sin(yaw + yawd * delta_t) - sin(yaw)) / yawd;
      py_p = py + v * (-cos(yaw + yawd * delta_t) + cos(yaw)) / yawd;
    }
    else {
      px_p = px + v * cos(yaw) * delta_t;
      py_p = py + v * sin(yaw) * delta_t;
    }
    double v_p = v + 0;
    double yaw_p = yaw + yawd * delta_t;
    double yawd_p = yawd + 0;

    //add noise
    px_p += delta_t * delta_t * cos(yaw) * nu_a * 0.5;
    py_p += delta_t * delta_t * sin(yaw) * nu_a * 0.5;
    v_p += delta_t * nu_a;
    yaw_p += delta_t * delta_t * nu_yawdd * 0.5;
    yawd_p += delta_t * nu_yawdd;

    //write predicted sigma point into right column
    VectorXd pred(n_x_);
    pred << px_p, py_p, v_p, yaw_p, yawd_p;
    Xsig_pred_.col(i) = pred;
  }

  /** Step3: Predicted Mean and Covariance
  */
  // 3.1. predict state mean
  x_ = Xsig_pred_ * weights_;

  // 3.2. predict state covariance matrix
  P_.fill(0.0);
  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    VectorXd dx = Xsig_pred_.col(i) - x_;
    while (dx(3) >  M_PI) dx(3) -= 2. * M_PI;
    while (dx(3) < -M_PI) dx(3) += 2. * M_PI;
    P_ = P_ + weights_(i) * dx * dx.transpose();
  }
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  /** Step4": Predict Lidar Measurement
  */
  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 2;
  // 4.1. Measurement Model
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    VectorXd x = Xsig_pred_.col(i);
    double px = x(0);
    double py = x(1);

    Zsig.col(i) << px, py;
  }

  // 4.2. calculate mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred = Zsig * weights_;

  // 4.3. calculate innovation covariance matrix S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);

  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    VectorXd zd = Zsig.col(i) - z_pred;
    S += weights_(i) * zd * zd.transpose();
  }
  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z, n_z);
  R << std_laspx_ * std_laspx_, 0,
       0, std_laspy_ * std_laspy_;
  S += R;

  /** Step5": UKF Update
  */
  // 5.1. calculate cross correlation matrix
  MatrixXd T = MatrixXd(n_x_, n_z);
  T.fill(0.0);
  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    VectorXd xd = Xsig_pred_.col(i) - x_;
    VectorXd zd = Zsig.col(i) - z_pred;

    //angle normalization
    while (xd(3) >  M_PI) xd(3) -= 2. * M_PI;
    while (xd(3) < -M_PI) xd(3) += 2. * M_PI;

    T += weights_(i) * xd * zd.transpose();
  }

  // 5.2. calculate Kalman gain K
  MatrixXd K = T * S.inverse();

  // 5.3. update state mean
  VectorXd zd = meas_package.raw_measurements_ - z_pred;
  x_ = x_ + K * zd;

  // 5.4. update covariance matrix
  P_ = P_ - K * S * K.transpose();

  // Step6": calculate NIS
  double epsilon = zd.transpose() * S.inverse() * zd;
  static double epsilon_sum = 0;
  epsilon_sum += epsilon;
  static int total = 0, over_cnt = 0;
  total++;
  over_cnt += epsilon > 5.991 ? 1 : 0;
  cout << "*************************** LIDAR " << total << " ****************************" << endl;
  cout << "Lidar NIS Cur: " << epsilon << endl;
  cout << "Lidar NIS Sum: " << epsilon_sum << endl;
  cout << "Lidar NIS Avg: " << epsilon_sum / total << endl;
  cout << "Lidar Data Total: " << total << endl;
  cout << "Lidar Data Over Threshold: " << over_cnt << endl;
  cout << "Lidar Consistency Rate: " << 100 - (double)(over_cnt) / (double)(total) << "%" << endl;
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */

  /** Step4': Predict Radar Measurement
  */
  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 3;
  // 4.1. Measurement Model
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    VectorXd x = Xsig_pred_.col(i);
    double px = x(0);
    double py = x(1);
    double v = x(2);
    double yaw = x(3);

    double ro = sqrt(px * px + py * py);
    double thi = atan2(py, px);
    double ro_d = (px * cos(yaw) * v + py * sin(yaw) * v) / ro;
    Zsig.col(i) << ro, thi, ro_d;
  }

  // 4.2. calculate mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred = Zsig * weights_;

  // 4.3. calculate innovation covariance matrix S
  MatrixXd S = MatrixXd(n_z, n_z);
  S.fill(0.0);

  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    VectorXd zd = Zsig.col(i) - z_pred;
    while (zd(1) >  M_PI) zd(1) -= 2. * M_PI;
    while (zd(1) < -M_PI) zd(1) += 2. * M_PI;
    S += weights_(i) * zd * zd.transpose();
  }
  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z, n_z);
  R << std_radr_ * std_radr_, 0, 0,
       0, std_radphi_ * std_radphi_, 0,
       0, 0, std_radrd_ * std_radrd_;
  S += R;

  /** Step5': UKF Update
  */
  // 5.1. calculate cross correlation matrix
  MatrixXd T = MatrixXd(n_x_, n_z);
  T.fill(0.0);
  for (auto i = 0; i != 2 * n_aug_ + 1; i++) {
    VectorXd xd = Xsig_pred_.col(i) - x_;
    VectorXd zd = Zsig.col(i) - z_pred;

    //angle normalization
    while (xd(3) >  M_PI) xd(3) -= 2. * M_PI;
    while (xd(3) < -M_PI) xd(3) += 2. * M_PI;

    //angle normalization
    while (zd(1) >  M_PI) zd(1) -= 2. * M_PI;
    while (zd(1) < -M_PI) zd(1) += 2. * M_PI;

    T += weights_(i) * xd * zd.transpose();
  }

  // 5.2. calculate Kalman gain K
  MatrixXd K = T * S.inverse();

  // 5.3. update state mean
  VectorXd zd = meas_package.raw_measurements_ - z_pred;
  while (zd(1) >  M_PI) zd(1) -= 2. * M_PI;
  while (zd(1) < -M_PI) zd(1) += 2. * M_PI;
  x_ = x_ + K * zd;

  // 5.4. update covariance matrix
  P_ = P_ - K * S * K.transpose();

  // Step6': calculate NIS
  double epsilon = zd.transpose() * S.inverse() * zd;
  static double epsilon_sum = 0;
  epsilon_sum += epsilon;
  static int total = 0, over_cnt = 0;
  total++;
  over_cnt += epsilon > 7.815 ? 1 : 0;
  cout << "*************************** RADAR " << total << " ****************************" << endl;
  cout << "Radar NIS Cur: " << epsilon << endl;
  cout << "Radar NIS Sum: " << epsilon_sum << endl;
  cout << "Radar NIS Avg: " << epsilon_sum / total << endl;
  cout << "Radar Data Total: " << total << endl;
  cout << "Radar Data Over Threshold: " << over_cnt << endl;
  cout << "Radar Consistency Rate: " << 100 - (double)(over_cnt) / (double)(total) << "%" << endl;
}
