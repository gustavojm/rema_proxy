#ifndef CIRCLE_FNS_HPP
#define CIRCLE_FNS_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#include "points.hpp"

struct Circle {
    Point3D center;
    double radius;
    double sigma, gradient;
    int iter;
};

static inline double distance(const Point3D &p1, const Point3D &p2) {
    return std::sqrt(
            (p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}

static inline double calculate_sigma(const std::vector<Point3D> &data,
        const Circle &circle) {
    double sum = 0.0f;
    double dx, dy;

    for (size_t i = 0; i < data.size(); i++) {
        dx = data[i].x - circle.center.x;
        dy = data[i].y - circle.center.y;
        double s = std::sqrt(dx * dx + dy * dy) - circle.radius;
        sum += (s * s);
    }
    return std::sqrt(sum / data.size());
}

static inline std::pair<double, double> calculate_means(
        const std::vector<Point3D> &data) {

    double mean_x = std::accumulate(data.begin(), data.end(), 0.0,
            [](double accumulator, const Point3D &point) {
                return accumulator + point.x;
            }) / data.size();

    double mean_y = std::accumulate(data.begin(), data.end(), 0.0,
            [](double accumulator, const Point3D &point) {
                return accumulator + point.y;
            }) / data.size();

    return {mean_x, mean_y};
}

/**
 * @brief   Algebraic circle fit to a given set of data points (in 2D)
 * @param   data : vector of circle points
 * @returns      : parameters of the fitting circle
 *
 * This is an algebraic fit based on the journal article
 * A. Al-Sharadqah and N. Chernov, "Error analysis for circle fitting algorithms",
 * Electronic Journal of Statistics, Vol. 3, pages 886-911, (2009)
 *
 * It is an algebraic circle fit with "hyperaccuracy" (with zero essential bias).
 * The term "hyperaccuracy" first appeared in papers by Kenichi Kanatani around 2006
 *
 * This method combines the Pratt and Taubin fits to eliminate the essential bias.
 *
 * It works well whether data points are sampled along an entire circle or
 * along a small arc.
 *
 * Its statistical accuracy is theoretically higher than that of the Pratt fit
 * and Taubin fit, but practically they all return almost identical circles
 * (unlike the Kasa fit that may be grossly inaccurate).
 *
 * It provides a very good initial guess for a subsequent geometric fit.
 *
 * @author: Nikolai Chernov  (September 2012)
 */
static inline Circle CircleFitByHyper(std::vector<Point3D> data) {
    int iter, IterMAX = 99;

    double Xi, Yi, Zi;
    double Mz, Mxy, Mxx, Myy, Mxz, Myz, Mzz, Cov_xy, Var_z;
    double A0, A1, A2, A22;
    double Dy, xnew, x, ynew, y;
    double DET, Xcenter, Ycenter;

    Circle circle;

    // Compute x and y sample means
    double mean_x = std::accumulate(data.begin(), data.end(), 0.0,
            [](double accumulator, const Point3D &point) {
                return accumulator + point.x;
            }) / data.size();

    double mean_y = std::accumulate(data.begin(), data.end(), 0.0,
            [](double accumulator, const Point3D &point) {
                return accumulator + point.y;
            }) / data.size();

    // Computing moments
    Mxx = Myy = Mxy = Mxz = Myz = Mzz = 0.;

    for (size_t i = 0; i < data.size(); i++) {
        Xi = data[i].x - mean_x;   //  centered x-coordinates
        Yi = data[i].y - mean_y;   //  centered y-coordinates
        Zi = Xi * Xi + Yi * Yi;

        Mxy += Xi * Yi;
        Mxx += Xi * Xi;
        Myy += Yi * Yi;
        Mxz += Xi * Zi;
        Myz += Yi * Zi;
        Mzz += Zi * Zi;
    }
    Mxx /= data.size();
    Myy /= data.size();
    Mxy /= data.size();
    Mxz /= data.size();
    Myz /= data.size();
    Mzz /= data.size();

    // Computing the coefficients of the characteristic polynomial
    Mz = Mxx + Myy;
    Cov_xy = Mxx * Myy - Mxy * Mxy;
    Var_z = Mzz - Mz * Mz;

    A2 = 4.0 * Cov_xy - 3.0 * Mz * Mz - Mzz;
    A1 = Var_z * Mz + 4.0 * Cov_xy * Mz - Mxz * Mxz - Myz * Myz;
    A0 = Mxz * (Mxz * Myy - Myz * Mxy) + Myz * (Myz * Mxx - Mxz * Mxy)
            - Var_z * Cov_xy;
    A22 = A2 + A2;

    // Finding the root of the characteristic polynomial using Newton's method starting at x=0
    // (it is guaranteed to converge to the right root)
    for (x = 0., y = A0, iter = 0; iter < IterMAX; iter++) // usually, 4-6 iterations are enough
            {
        Dy = A1 + x * (A22 + 16. * x * x);
        xnew = x - y / Dy;
        if ((xnew == x) || (!std::isfinite(xnew)))
            break;
        ynew = A0 + xnew * (A1 + xnew * (A2 + 4.0 * xnew * xnew));
        if (std::abs(ynew) >= std::abs(y))
            break;
        x = xnew;
        y = ynew;
    }

    // Computing paramters of the fitting circle

    DET = x * x - x * Mz + Cov_xy;
    Xcenter = (Mxz * (Myy - x) - Myz * Mxy) / DET / 2.0;
    Ycenter = (Myz * (Mxx - x) - Mxz * Mxy) / DET / 2.0;

    circle.center.x = Xcenter + mean_x;
    circle.center.y = Ycenter + mean_y;
    circle.radius = std::sqrt(
            Xcenter * Xcenter + Ycenter * Ycenter + Mz - x - x);
    circle.sigma = calculate_sigma(data, circle);
    circle.iter = iter;  //  return the number of iterations, too

    return circle;
}

/**
 * @brief   Algebraic circle fit to a given set of data points (in 2D)
 * @param   data : vector of circle points
 * @returns      : parameters of the fitting circle
 *
 * This is an algebraic fit, disovered and rediscovered by many people.
 * One of the earliest publications is due to Kasa:
 * I. Kasa, "A curve fitting procedure and its error analysis",
 * IEEE Trans. Inst. Meas., Vol. 25, pages 8-14, (1976)
 * The method is based on the minimization of the function
 *
 * F = sum [(x-a)^2 + (y-b)^2 - R^2]^2
 *
 *This is perhaps the simplest and fastest circle fit.
 *
 *It works well when data points are sampled along an entire circle
 *or a large part of it (at least half circle).
 *
 *It does not work well when data points are sampled along a small arc
 *of a circle. In that case the method is heavily biased, it returns
 *circles that are too often too small.
 *
 *It is the oldest algebraic circle fit (first published in 1972?).
 *For 20-30 years it has been the most popular circle fit, at least
 *until the more robust Pratt fit (1987) and Taubin fit (1991) were invented.
 *
 *@author: Nikolai Chernov  (September 2012)
 */
static inline Circle CircleFitByKasa(std::vector<Point3D> &data) {
    double Xi, Yi, Zi;
    double Mxy, Mxx, Myy, Mxz, Myz;
    double B, C, G11, G12, G22, D1, D2;

    Circle circle;

    // Compute x and y sample means
    auto [mean_x, mean_y] = calculate_means(data);

    // Computing moments
    Mxx = Myy = Mxy = Mxz = Myz = 0.;

    for (size_t i = 0; i < data.size(); i++) {
        Xi = data[i].x - mean_x;   //  centered x-coordinates
        Yi = data[i].y - mean_y;   //  centered y-coordinates
        Zi = Xi * Xi + Yi * Yi;

        Mxx += Xi * Xi;
        Myy += Yi * Yi;
        Mxy += Xi * Yi;
        Mxz += Xi * Zi;
        Myz += Yi * Zi;
    }
    Mxx /= data.size();
    Myy /= data.size();
    Mxy /= data.size();
    Mxz /= data.size();
    Myz /= data.size();

    // Solving system of equations by Cholesky factorization
    G11 = std::sqrt(Mxx);
    G12 = Mxy / G11;
    G22 = std::sqrt(Myy - G12 * G12);

    D1 = Mxz / G11;
    D2 = (Myz - D1 * G12) / G22;

    // Computing paramters of the fitting circle
    C = D2 / G22 / 2.0;
    B = (D1 - G12 * C) / G11 / 2.0;

    circle.center.x = B + mean_x;
    circle.center.y = C + mean_y;
    circle.radius = std::sqrt(B * B + C * C + Mxx + Myy);
    circle.sigma = calculate_sigma(data, circle);
    circle.iter = 0;

    return circle;
}

/**
 * @brief   Algebraic circle fit to a given set of data points (in 2D)
 * @param   data : vector of circle points
 * @returns      : parameters of the fitting circle
 *
 * This is an algebraic fit, due to Pratt, based on the journal article
 * V. Pratt, "Direct least-squares fitting of algebraic surfaces",
 * Computer Graphics, Vol. 21, pages 145-152 (1987)
 * The method is based on the minimization of the function
 *
 * F = sum [(x-a)^2 + (y-b)^2 - R^2]^2 / R^2
 *
 * This method is more balanced than the simple Kasa fit.
 *
 * It works well whether data points are sampled along an entire circle or
 * along a small arc.
 *
 * It still has a small bias and its statistical accuracy is slightly
 * lower than that of the geometric fit (minimizing geometric distances).
 *
 * It provides a good initial guess for a subsequent geometric fit.
 * @author: Nikolai Chernov  (September 2012)
 */
static inline Circle CircleFitByPratt(const std::vector<Point3D> &data) {
    int iter, IterMAX = 99;

    double Xi, Yi, Zi;
    double Mz, Mxy, Mxx, Myy, Mxz, Myz, Mzz, Cov_xy, Var_z;
    double A0, A1, A2, A22;
    double Dy, xnew, x, ynew, y;
    double DET, Xcenter, Ycenter;

    Circle circle;

    // Compute x and y sample means
    auto [mean_x, mean_y] = calculate_means(data);

    // Computing moments
    Mxx = Myy = Mxy = Mxz = Myz = Mzz = 0.;

    for (size_t i = 0; i < data.size(); i++) {
        Xi = data[i].x - mean_x;   //  centered x-coordinates
        Yi = data[i].y - mean_y;   //  centered y-coordinates
        Zi = Xi * Xi + Yi * Yi;

        Mxy += Xi * Yi;
        Mxx += Xi * Xi;
        Myy += Yi * Yi;
        Mxz += Xi * Zi;
        Myz += Yi * Zi;
        Mzz += Zi * Zi;
    }
    Mxx /= data.size();
    Myy /= data.size();
    Mxy /= data.size();
    Mxz /= data.size();
    Myz /= data.size();
    Mzz /= data.size();

    // Computing coefficients of the characteristic polynomial
    Mz = Mxx + Myy;
    Cov_xy = Mxx * Myy - Mxy * Mxy;
    Var_z = Mzz - Mz * Mz;

    A2 = 4.0 * Cov_xy - 3.0 * Mz * Mz - Mzz;
    A1 = Var_z * Mz + 4.0 * Cov_xy * Mz - Mxz * Mxz - Myz * Myz;
    A0 = Mxz * (Mxz * Myy - Myz * Mxy) + Myz * (Myz * Mxx - Mxz * Mxy)
            - Var_z * Cov_xy;
    A22 = A2 + A2;

    // Finding the root of the characteristic polynomial using Newton's method starting at x=0
    // (it is guaranteed to converge to the right root)
    for (x = 0., y = A0, iter = 0; iter < IterMAX; iter++) // usually, 4-6 iterations are enough
            {
        Dy = A1 + x * (A22 + 16. * x * x);
        xnew = x - y / Dy;
        if ((xnew == x) || (!std::isfinite(xnew)))
            break;
        ynew = A0 + xnew * (A1 + xnew * (A2 + 4.0 * xnew * xnew));
        if (std::abs(ynew) >= std::abs(y))
            break;
        x = xnew;
        y = ynew;
    }

    // Computing paramters of the fitting circle
    DET = x * x - x * Mz + Cov_xy;
    Xcenter = (Mxz * (Myy - x) - Myz * Mxy) / DET / 2.0;
    Ycenter = (Myz * (Mxx - x) - Mxz * Mxy) / DET / 2.0;

    circle.center.x = Xcenter + mean_x;
    circle.center.y = Ycenter + mean_y;
    circle.radius = std::sqrt(
            Xcenter * Xcenter + Ycenter * Ycenter + Mz + x + x);
    circle.sigma = calculate_sigma(data, circle);
    circle.iter = iter;  //  return the number of iterations, too

    return circle;
}

/**
 * @brief   Algebraic circle fit to a given set of data points (in 2D)
 * @param   data : vector of circle points
 * @returns      : parameters of the fitting circle
 *
 * This is an algebraic fit, due to Taubin, based on the journal article
 * G. Taubin, "Estimation Of Planar Curves, Surfaces And Nonplanar
 * Space Curves Defined By Implicit Equations, With
 * Applications To Edge And Range Image Segmentation",
 * IEEE Trans. PAMI, Vol. 13, pages 1115-1138, (1991)
 * The method is based on the minimization of the function
 *
 *     sum [(x-a)^2 + (y-b)^2 - R^2]^2
 * F = -------------------------------
 *       sum [(x-a)^2 + (y-b)^2]
 *
 * This method is more balanced than the simple Kasa fit.
 * It works well whether data points are sampled along an entire circle or
 * along a small arc.
 * It still has a small bias and its statistical accuracy is slightly
 * lower than that of the geometric fit (minimizing geometric distances),
 * but slightly higher than that of the very similar Pratt fit.
 * Besides, the Taubin fit is slightly simpler than the Pratt fit
 * It provides a very good initial guess for a subsequent geometric fit.
 *
 * @author: Nikolai Chernov  (September 2012)
 */
static inline Circle CircleFitByTaubin(std::vector<Point3D> data) {
    int iter, IterMAX = 99;

    double Xi, Yi, Zi;
    double Mz, Mxy, Mxx, Myy, Mxz, Myz, Mzz, Cov_xy, Var_z;
    double A0, A1, A2, A22, A3, A33;
    double Dy, xnew, x, ynew, y;
    double DET, Xcenter, Ycenter;

    Circle circle;

    // Compute x and y sample means
    auto [mean_x, mean_y] = calculate_means(data);

    // Computing moments
    Mxx = Myy = Mxy = Mxz = Myz = Mzz = 0.;

    for (size_t i = 0; i < data.size(); i++) {
        Xi = data[i].x - mean_x;   //  centered x-coordinates
        Yi = data[i].y - mean_y;   //  centered y-coordinates
        Zi = Xi * Xi + Yi * Yi;

        Mxy += Xi * Yi;
        Mxx += Xi * Xi;
        Myy += Yi * Yi;
        Mxz += Xi * Zi;
        Myz += Yi * Zi;
        Mzz += Zi * Zi;
    }
    Mxx /= data.size();
    Myy /= data.size();
    Mxy /= data.size();
    Mxz /= data.size();
    Myz /= data.size();
    Mzz /= data.size();

    // Computing coefficients of the characteristic polynomial
    Mz = Mxx + Myy;
    Cov_xy = Mxx * Myy - Mxy * Mxy;
    Var_z = Mzz - Mz * Mz;
    A3 = 4.0 * Mz;
    A2 = -3.0 * Mz * Mz - Mzz;
    A1 = Var_z * Mz + 4.0 * Cov_xy * Mz - Mxz * Mxz - Myz * Myz;
    A0 = Mxz * (Mxz * Myy - Myz * Mxy) + Myz * (Myz * Mxx - Mxz * Mxy)
            - Var_z * Cov_xy;
    A22 = A2 + A2;
    A33 = A3 + A3 + A3;

    // Finding the root of the characteristic polynomial using Newton's method starting at x=0
    // (it is guaranteed to converge to the right root)
    for (x = 0., y = A0, iter = 0; iter < IterMAX; iter++) // usually, 4-6 iterations are enough
            {
        Dy = A1 + x * (A22 + A33 * x);
        xnew = x - y / Dy;
        if ((xnew == x) || (!std::isfinite(xnew)))
            break;
        ynew = A0 + xnew * (A1 + xnew * (A2 + xnew * A3));
        if (std::abs(ynew) >= std::abs(y))
            break;
        x = xnew;
        y = ynew;
    }

    // Computing paramters of the fitting circle
    DET = x * x - x * Mz + Cov_xy;
    Xcenter = (Mxz * (Myy - x) - Myz * Mxy) / DET / 2.0;
    Ycenter = (Myz * (Mxx - x) - Mxz * Mxy) / DET / 2.0;

    circle.center.x = Xcenter + mean_x;
    circle.center.y = Ycenter + mean_y;
    circle.radius = std::sqrt(Xcenter * Xcenter + Ycenter * Ycenter + Mz);
    circle.sigma = calculate_sigma(data, circle);
    circle.iter = iter;  //  return the number of iterations, too

    return circle;
}

/**
 * @brief   Geometric circle fit to a given set of data points (in 2D)
 * @param   data      : vector of circle points
 *          circleIni : parameters of the initial circle ("initial guess")
 *          LambdaIni : the initial value of the control parameter "lambda" for the Levenberg-Marquardt procedure
 *                       (common choice is a small positive number, e.g. 0.001)
 *
 * @returns : code, Circle
 *            where "code" :
 *                        0: normal termination, the best fitting circle is successfully found
 *                        1: the number of outer iterations exceeds the limit (99)  (indicator of a possible divergence)
 *                        2: the number of inner iterations exceeds the limit (99) (another indicator of a possible divergence)
 *                        3: the coordinates of the center are too large (a strong indicator of divergence)
 *             circle : parameters of the fitting circle ("best fit")
 * Algorithm:  Levenberg-Marquardt running over the full parameter space (a,b,r)
 *
 * See a detailed description in Section 4.5 of the book by Nikolai Chernov:
 * "Circular and linear regression: Fitting circles and lines by least squares"
 * Chapman & Hall/CRC, Monographs on Statistics and Applied Probability, volume 117, 2010.
 *
 * @author: Nikolai Chernov  (September 2012)
 */
static inline std::pair<int, Circle> CircleFitByLevenbergMarquardtFull(
        const std::vector<Point3D> &data, const Circle &circleIni,
        double LambdaIni,[[maybe_unused]] Circle &circle) {
    int code, iter, inner, IterMAX = 99;
    double factorUp = 10., factorDown = 0.04, lambda, ParLimit = 1.e+6;
    double dx, dy, ri, u, v;
    double Mu, Mv, Muu, Mvv, Muv, Mr, UUl, VVl, Nl, F1, F2, F3, dX, dY, dR;
    double epsilon = 3.e-8;
    double G11, G22, G33, G12, G13, G23, D1, D2, D3;

    // Compute x and y sample means
    auto [mean_x, mean_y] = calculate_means(data);

    Circle Old, New;

    // Starting with the given initial circle (initial guess)
    New = circleIni;

    // Compute the root-mean-square error via function calculate_sigma; see Utilities.cpp
    New.sigma = calculate_sigma(data, New);

    // Initializing lambda, iteration counters, and the exit code
    lambda = LambdaIni;
    iter = inner = code = 0;

    NextIteration:

    Old = New;
    if (++iter > IterMAX) {
        code = 1;
        goto enough;
    }

    // Computing moments
    Mu = Mv = Muu = Mvv = Muv = Mr = 0.;

    for (size_t i = 0; i < data.size(); i++) {
        dx = data[i].x - Old.center.x;
        dy = data[i].y - Old.center.y;
        ri = std::sqrt(dx * dx + dy * dy);
        u = dx / ri;
        v = dy / ri;
        Mu += u;
        Mv += v;
        Muu += u * u;
        Mvv += v * v;
        Muv += u * v;
        Mr += ri;
    }
    Mu /= data.size();
    Mv /= data.size();
    Muu /= data.size();
    Mvv /= data.size();
    Muv /= data.size();
    Mr /= data.size();

    // Computing matrices
    F1 = Old.center.x + Old.radius * Mu - mean_x;
    F2 = Old.center.y + Old.radius * Mv - mean_y;
    F3 = Old.radius - Mr;

    Old.gradient = New.gradient = std::sqrt(F1 * F1 + F2 * F2 + F3 * F3);

    try_again:

    UUl = Muu + lambda;
    VVl = Mvv + lambda;
    Nl = 1.0 + lambda;

    // Cholesly decomposition
    G11 = std::sqrt(UUl);
    G12 = Muv / G11;
    G13 = Mu / G11;
    G22 = std::sqrt(VVl - G12 * G12);
    G23 = (Mv - G12 * G13) / G22;
    G33 = std::sqrt(Nl - G13 * G13 - G23 * G23);

    D1 = F1 / G11;
    D2 = (F2 - G12 * D1) / G22;
    D3 = (F3 - G13 * D1 - G23 * D2) / G33;

    dR = D3 / G33;
    dY = (D2 - G23 * dR) / G22;
    dX = (D1 - G12 * dY - G13 * dR) / G11;

    if ((std::abs(dR) + std::abs(dX) + std::abs(dY)) / (1.0 + Old.radius)
            < epsilon)
        goto enough;

    // Updating the parameters
    New.center.x = Old.center.x - dX;
    New.center.y = Old.center.y - dY;

    if (std::abs(New.center.x) > ParLimit
            || std::abs(New.center.y) > ParLimit) {
        code = 3;
        goto enough;
    }

    New.radius = Old.radius - dR;

    if (New.radius <= 0.) {
        lambda *= factorUp;
        if (++inner > IterMAX) {
            code = 2;
            goto enough;
        }
        goto try_again;
    }

    // Compute the root-mean-square error via function calculate_sigma; see Utilities.cpp
    New.sigma = calculate_sigma(data, New);

    // Check if improvement is gained
    if (New.sigma < Old.sigma)    //   yes, improvement
            {
        lambda *= factorDown;
        goto NextIteration;
    } else {                      //   no improvement
        if (++inner > IterMAX) {
            code = 2;
            goto enough;
        }
        lambda *= factorUp;
        goto try_again;
    }

    enough:
    return {code, New};
}

static inline std::vector<Point3D> calculateCirclePoints(Point3D center,
        double radius, int numPoints) {
    std::vector<Point3D> points;

    double angleIncrement = 360.0 / numPoints;

    for (int i = 0; i < numPoints; ++i) {
        double angle = i * angleIncrement;
        double radians = angle * (M_PI / 180.0); // Convert degrees to radians

        Point3D point;
        point.x = center.x + radius * cos(radians);
        point.y = center.y + radius * sin(radians);

        points.push_back( { point.x, point.y, 0 });
        //std::cout << "Point " << i + 1 << ": (" << point.x << ", " << point.y << ")\n";
    }
    return points;
}

#endif     // CIRCLE_FNS_HPP
