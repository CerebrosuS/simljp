/* Copyright 2017 <Christian Krippendorf>
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include <iostream>
#include <mkl.h>
#include <eigen3/Eigen/Dense>
#include <cmath>
#include <random>
#include <sys/stat.h>
#include <ctime>
#include <fstream>

#define EIGEN_USE_MKL_ALL

// Cofficients for the Lennard-Jones potential.
#define SIGMA 1.0e-1
#define EPSILON 1.0

// The mass of an atom. /kg
#define MASS 1

// Total number of particles to simulate.
#define TOTAL_PARTICLE 64

// Total number of simulation loops.
#define TOTAL_TIMESTEPS 1e6

// Single timestep for integration. /s
#define TIMESTEP 1e-6

using namespace Eigen;
using namespace std;

// Define csv format for eigen
const static IOFormat CSVFormat(StreamPrecision, DontAlignCols, ", ", "\n");

// Constant variables and information.
const char * const __version__  = "1.0";
const char * const __author__   = "Christian Krippendorf";
const char * const __email__    = "Coding@Christian-Krippendorf.de";

/** Manipulate the position and velocity matrices for border conditions.
 *
 *  mp:     Reference to the position matrix of all particles. /m
 *  mv:     Reference to the velocity matrix of all particles. /(m/s)
 *  closed: True if a limited and closed box should be simulated, else false.
 *          If it is not closed an algorithm put every particle on the opposit
 *          site on reaching the border.
 *  left:   Left border of the box. /m
 *  right:  Right border of the box. /m
 *  top:    Top border of the box. /m
 *  bottom: Bottom border of the box. /m
 *  front:  Front border of the box. /m
 *  back:   Back border of the box. /m
 *
 *  TODO:   Algorithm for a not closed system. */
void border_handling(MatrixXd &mp, MatrixXd &mv, bool closed, double left,
  double right, double top, double bottom, double front, double back) {
  if (closed) {
    // If one of the particles reaches the end of the box, the velocity has to
    // be reverted (multiplication with -1). The only problem is to decide with
    // component of the vector has to be inverted.

    // Go throught all particle and search for a position which is outside the
    // box.
    for (int pi = 0; pi < mp.cols(); pi++) {
      if (mp(0, pi) > right || mp(0, pi) < left)
        mv(0, pi) *= -1;

      if (mp(1, pi) > top || mp(1, pi) < bottom)
        mv(1, pi) *= -1;

      if (mp(2, pi) > back || mp(2, pi) < front)
        mv(2, pi) *= -1;
    }
  }
}

/** Initialize the velocities of the particles.
 *
 *  The velocities of the particles follow the Boltzmann-Maxwell distribution.
 *  This is just another version of component-wise normal distribution, which
 *  will be implemented here.
 *
 *  mv:   Reference to the velocity matrix of all particles.
 *
 *  TODO: This function is not fully implemented to the temperature of the
 *        system and needs further programming. */
void init_velocities(MatrixXd &mv) {
  // Total number of columns (particles).
  int co = mv.cols();

  // Create the normal distribution object for generating random velocity
  // numbers.
  default_random_engine generator;
  normal_distribution<double> dist(0.0, 2.0);

  // Calculate velocity components for every particle.
  for (int pi = 0; pi < co; pi++) {
    mv(0, pi) = dist(generator);
    mv(1, pi) = dist(generator);
    mv(2, pi) = dist(generator);
  }
}

/** Initialize the positions of all particles.
 *
 *  The particles will be positioned like equal distanced particles in a
 *  cube. Therefore the number of total particles should be the third power of
 *  any natural number.
 *
 *  mp:   Reference to the position matrix of all particles.
 *
 *  TODO: Handle different total numbers of particles and not only a third
 *        power of natural numbers. */
void init_grid(MatrixXd &mp) {
  // Position variables for counting over the loops.
  int px = 0, py = 0, pz = 0;

  // Total number of columns (particles).
  int co = mp.cols();

  // The number of rows gives the dimension. The number of columns gives the
  // number of all particles. The number of particles per dimension side
  // should be the dimension root of the particle number. Otherwise the number
  // of particles is wrong.
  double po = cbrt(co);
  if (fmod(po, 1) != 0)
    cout << "Error: Wrong size of particles." << endl;

  // Got through all particle postitions and give them a position number.
  for (int pi = 0; pi < co; pi++) {
    mp(0, pi) = px;
    mp(1, pi) = py;
    mp(2, pi) = pz;

    // If the x position is a multiple of po value, reset the px value to
    // zero and increase the y position. The same calculation follows with
    // y and z position. */
    px++;
    if ((px % (int)po) == 0) {
      px = 0;
      py++;
    }
    if (py != 0 && (py % (int)po) == 0) {
      py = 0;
      pz++;
    }
  }
}

/** Calculate the Lennard-Jones potential energy force for all particles.
 *
 *  vp:  Reference to the vector object of the particle to calculate the final
 *       force for.
 *  mp:  Reference to the matrix object of all surrounding particles.
 *  mpo: Reference to the matrix object where the final force will be stored. */
void calc_lenjon_force(const VectorXd &vp, const MatrixXd &mp, MatrixXd &mpo) {
  // Get distance between the main particle and all surrounding particles.
  MatrixXd rp = mp-vp.replicate(1, mp.cols());

  // Calculate the distance of the particles by the norm.
  VectorXd rpn0 = rp.colwise().norm().cwiseInverse();

  // Calculate the resulting forces as magnitude.
  VectorXd rpn1 = rpn0*SIGMA;
  rpn1 = 24*EPSILON*(2*rpn1.array().pow(7.0)-rpn1.array().pow(13.0));

  // Go back to the component wise view.
  mpo = rp.array().rowwise()*rpn1.cwiseProduct(rpn0).transpose().array();
}

/** Calculation of the particle accelerations based on the resulting forces.
 *
 *  mp: Matrix object for the positions with 3 rows and n columns.
 *  ma: Matrix object for accelerations with 3 rows and n columns. */
void calc_accel(const MatrixXd &mp, MatrixXd &ma) {
  // Number of columns (particles).
  int co = mp.cols();

  // Temporary vector/matrix objectes for calculation.
  VectorXd vf(3, 1);
  MatrixXd mpo(3, co);

  for (int pi = 0; pi < co; pi++) {
    VectorXd vp = mp.col(pi);
    calc_lenjon_force(vp, mp.block(0, pi+1, 3, co-pi-1), mpo);
    ma.col(pi) = mpo.rowwise().sum();
  }
}

/** Test whether a path exist or not.
 *
 *  Return: True if path exist, else false. */
bool path_exist(const char* path) {
  struct stat my_stat;
  return (stat(path, &my_stat) == 0);
}

/** Initialize serialization.
 *
 *  Search for a saving path and create it if neccessary. This method should be
 *  optimized throught a configuration file.
 *
 *  Return: Name of the output path. */
string init_serialize() {
  // Time data object for getting the raw data.
  time_t rawtime;
  struct tm *timeinfo;

  // String containing the time information in the right format.
  char tbuf[80];

  // Get current datetime from the time object.
  time(&rawtime);
  timeinfo = localtime(&rawtime);

  // Convert the datetime information to string.
  strftime(tbuf, sizeof(tbuf), "%d-%m-%Y_%I-%M-%S", timeinfo);

  // Create final path as string with prefix.
  string path = string("mds-") + string(tbuf) + string("/");
  mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP |
    S_IXGRP);

  return path;
}

/** Write the given matrices to file.
 *
 *  Get all references to the matrices and write them into a separate csv file
 *  in the given path.
 *
 *  mp:    Matrix object for the positions with 3 rows and n columns.
 *  ma:    Matrix object for accelerations with 3 rows and n columns.
 *  mv:    Matrix object for velocties with 3 rows and n columns.
 *  count: Number of loop; This gives information about the number of file to
 *         write in. */
void write(MatrixXd &mp, MatrixXd &mv, MatrixXd &ma, string path, int count) {
  // Open the output stream.
  ofstream out((path + string("/mds-") + to_string(count) +
    string(".csv")).c_str());

  // Write data into the stream in an appropriate data format.
  out << mp.transpose().format(CSVFormat);

  // Close the output stream.
  out.close();
}

/** Simulate the system by calculation with velocity verlet algorithm.
 *
 *  mp:  Reference to the position matrix of all particles.
 *  mv:  Reference to the velocity matrix of all particles.
 *  ma:  Reference to the acceleration matrix of all particles.
 *  td:  Timestep for every integration loop. /s
 *  tts: Total number of loops/timesteps to integrate/simulate. */
void simulate(MatrixXd &mp, MatrixXd &mv, MatrixXd &ma, double td, double tts,
  bool serialize) {
  // If serialization is wanted. Initialize the system to do so.
  string path;
  if (serialize)
    path = init_serialize();

  // Calculate box borders from number of particles.
  double po = cbrt(mp.cols());
  if (fmod(po, 1) != 0)
    cout << "Error: Wrong size of particles." << endl;

  // Calculate accelerations
  calc_accel(mp, ma);

  // Main timestep loop
  for (int ts = 0; ts < tts; ts++) {
    // Implementation of the Störmer-Velocity-Verlet algorithm.
    mp = mp + mv*td + 0.5*ma*pow(td, 2);
    MatrixXd mal = ma;
    calc_accel(mp, ma);
    ma += mal;
    mv += 0.5*ma*td;

    // Correct the velocities and/or positions related to the way of handling
    // border conditions.
    border_handling(mp, mv, true, 0, po, 0, po, 0, po);

    // Write current state to file.
    if (serialize)
      write(mp, mv, ma, path, ts);
  }
}

/** Write short information about hte application. */
void info() {
  cout << "Molecular Dynamic Simulation (Ver. " << __version__ << ")" << endl
    << "by " << __author__ << " <" << __email__ << ">" << endl;
}

/** Main entry function. */
int main(int argc, char **argv) {
    // Print application starting information
    info();

    // Define all system properties which are important to run the simulation.
    // This part should be changed by the user in order to make adjustments to
    // the simulation.

    // Total time as integration loops
    double tts = TOTAL_TIMESTEPS;

    // Timstep for integration
    double td = TIMESTEP;

    // Number of total particles in the system.
    unsigned int pn = TOTAL_PARTICLE;

    // Define matrices of position, velocity and acceleration.
    MatrixXd mp(3, pn), mv(3, pn), ma(3, pn);

    // Initialize the position and velocity matrices, cause they are needed for
    // integration.
    init_grid(mp);
    init_velocities(mv);

    // Start the main simulation process.
    simulate(mp, mv, ma, td, tts, true);

    return 0;
}