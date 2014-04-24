// g2o - General Graph Optimization
// Copyright (C) 2011 H. Strasdat
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef G2O_SIX_DOF_TYPES_EXPMAP
#define G2O_SIX_DOF_TYPES_EXPMAP

#include "g2o/core/base_vertex.h"
#include "g2o/core/base_binary_edge.h"
//#include "g2o/core/base_unary_edge.h"
#include "g2o/types/slam3d/se3_ops.h"
#include "types_sba.h"
#include <Eigen/Geometry>

namespace g2o {
namespace types_six_dof_expmap {
void init();
}

using namespace Eigen;

typedef Eigen::Matrix<double, 6, 6> Matrix6d;
typedef Eigen::Matrix<double, 1, 1> Vector1;

class CameraParameters : public g2o::Parameter
{
public:

  CameraParameters();
  CameraParameters(double focal_length,
                   const Vector2d & principle_point,
                   double baseline)
    : focal_length(focal_length),
      principle_point(principle_point),
      baseline(baseline){}

  CameraParameters(const Eigen::Vector2d & _focal_length,
                   const Eigen::Vector2d & principle_point,
                   double distortion)
    : _focal_length(_focal_length),
      principle_point(principle_point),
      distortion(distortion){}

 /* CameraParameters( double _focal_length,
                   const Eigen::Vector2d & principle_point,
                   double baseline, double distortion)
    : focal_length(_focal_length),
      principle_point(principle_point),
      baseline(baseline),
      distortion(distortion){
        std::cerr<<"g2o::camera parameters"<<focal_length<<" "<<principle_point<<" "<<distortion<<std::endl;
  }*/

  Vector2d cam_map (const Vector3d & trans_xyz) const;
  Vector2d cam_map_distortion (const Vector3d & trans_xyz) const;

  Vector3d stereocam_uvu_map (const Vector3d & trans_xyz) const;

  virtual bool read (std::istream& is){
    is >> focal_length;
    is >> principle_point[0];
    is >> principle_point[1];
    is >> baseline;
    return true;
  }

  virtual bool write (std::ostream& os) const {
    os << focal_length << " ";
    os << principle_point.x() << " ";
    os << principle_point.y() << " ";
    os << baseline << " ";
    return true;
  }

  double focal_length;
  Eigen::Vector2d principle_point;
  Eigen::Vector2d _focal_length;
  double baseline;
  double distortion;

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
};

/**
 * \brief SE3 Vertex parameterized internally with a transformation matrix
 and externally with its exponential map
 */
class G2O_TYPES_SBA_API VertexSE3Expmap : public BaseVertex<6, SE3Quat>{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  VertexSE3Expmap();

  bool read(std::istream& is);

  bool write(std::ostream& os) const;

  virtual void setToOriginImpl() {
    _estimate = SE3Quat();
  }

  virtual void oplusImpl(const double* update_)  {
    Eigen::Map<const Vector6d> update(update_);
    setEstimate(SE3Quat::exp(update)*estimate());
  }
};


/**
 * \brief 6D edge between two Vertex6
 */
class G2O_TYPES_SBA_API EdgeSE3Expmap : public BaseBinaryEdge<6, SE3Quat, VertexSE3Expmap, VertexSE3Expmap>{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  EdgeSE3Expmap();

  bool read(std::istream& is);

  bool write(std::ostream& os) const;

  void computeError()  {
    const VertexSE3Expmap* v1 = static_cast<const VertexSE3Expmap*>(_vertices[0]);
    const VertexSE3Expmap* v2 = static_cast<const VertexSE3Expmap*>(_vertices[1]);

    SE3Quat C(_measurement);
    SE3Quat error_= v2->estimate().inverse()*C*v1->estimate();
    _error = error_.log();
  }

  virtual void linearizeOplus();
};


class G2O_TYPES_SBA_API EdgeProjectXYZ2UV : public  BaseBinaryEdge<2, Vector2d, VertexSBAPointXYZ, VertexSE3Expmap>{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  EdgeProjectXYZ2UV();

  bool read(std::istream& is);

  bool write(std::ostream& os) const;

  void computeError()  {
    const VertexSE3Expmap* v1 = static_cast<const VertexSE3Expmap*>(_vertices[1]);
    const VertexSBAPointXYZ* v2 = static_cast<const VertexSBAPointXYZ*>(_vertices[0]);
    const CameraParameters * cam
        = static_cast<const CameraParameters *>(parameter(0));
    Vector2d obs(_measurement);
    _error = obs-cam->cam_map(v1->estimate().map(v2->estimate()));
  }

  virtual void linearizeOplus();

  CameraParameters * _cam;
};


class G2O_TYPES_SBA_API EdgeProjectPSI2UV : public  g2o::BaseMultiEdge<2, Vector2d>
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  EdgeProjectPSI2UV()  {
    resizeParameters(1);
    installParameter(_cam, 0);
  }

  virtual bool read  (std::istream& is);
  virtual bool write (std::ostream& os) const;
  void computeError  ();
  virtual void linearizeOplus ();
  CameraParameters * _cam;
};



//Stereo Observations
// U: left u
// V: left v
// U: right u
class G2O_TYPES_SBA_API EdgeProjectXYZ2UVU : public  BaseBinaryEdge<3, Vector3d, VertexSBAPointXYZ, VertexSE3Expmap>{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  EdgeProjectXYZ2UVU();

  bool read(std::istream& is);

  bool write(std::ostream& os) const;

  void computeError(){
    const VertexSE3Expmap* v1 = static_cast<const VertexSE3Expmap*>(_vertices[1]);
    const VertexSBAPointXYZ* v2 = static_cast<const VertexSBAPointXYZ*>(_vertices[0]);
    const CameraParameters * cam
        = static_cast<const CameraParameters *>(parameter(0));
    Vector3d obs(_measurement);
    _error = obs-cam->stereocam_uvu_map(v1->estimate().map(v2->estimate()));
  }
  //  virtual void linearizeOplus();
};

class G2O_TYPES_SBA_API EdgeProjectXYZ2UVdist : public  BaseBinaryEdge<2, Vector2d, VertexSBAPointXYZ, VertexSE3Expmap>
{
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    EdgeProjectXYZ2UVdist();
    bool read(std::istream& is);
    bool write(std::ostream& os) const;

    void computeError()  {

      const VertexSE3Expmap* v1 = static_cast<const VertexSE3Expmap*>(_vertices[1]);
      const VertexSBAPointXYZ* v2 = static_cast<const VertexSBAPointXYZ*>(_vertices[0]);
      const CameraParameters * cam = static_cast<const CameraParameters *>(parameter(0));
      Vector2d obs(_measurement);
      _error = obs-cam->cam_map_distortion(v1->estimate().map(v2->estimate()));
    }

    virtual void linearizeOplus();

    CameraParameters * _cam;
};

//class G2O_TYPES_SBA_API EdgeKinectDistance : public  BaseUnaryEdge<3, Vector3d, VertexSBAPointXYZ>
//{
//  public:
//    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
//    EdgeKinectDistance(){};
//  virtual bool read(std::istream& /*is*/)
//  {
//    return false;
//  }
//  
//  virtual bool write(std::ostream& /*os*/) const
//  {
//    return false;
//  }
//
//    void computeError()  {
//
//      const VertexSBAPointXYZ* v1 = static_cast<const VertexSBAPointXYZ*>(_vertices[0]);
//      _error = _measurement - v1->estimate();
//    }
//
////    virtual void linearizeOplus();
//
//};


class G2O_TYPES_SBA_API EdgeKinectDistance : public  BaseBinaryEdge<3, Vector3d, VertexSBAPointXYZ,VertexSBAPointXYZ>
{
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    EdgeKinectDistance(){};
  virtual bool read(std::istream& /*is*/)
  {
    return false;
  }
  
  virtual bool write(std::ostream& /*os*/) const
  {
    return false;
  }

    void computeError()  {

      const VertexSBAPointXYZ* v1 = static_cast<const VertexSBAPointXYZ*>(_vertices[0]);
      _error = _measurement - v1->estimate();
    }

//    virtual void linearizeOplus();

};
















 class VertexScale : public BaseVertex<1, Vector1>
 {
    public:
      EIGEN_MAKE_ALIGNED_OPERATOR_NEW

      VertexScale(double s) :s(s) { setToOriginImpl();}

      virtual bool read(std::istream& is)  { return true; }
      virtual bool write(std::ostream& os) const  { return true; }

      virtual void setToOriginImpl()
      {
        _estimate[0] = s;
      }

      virtual void oplusImpl(const double* update_)
      {
        _estimate[0] += update_[0];
      }

      double get_scale()
      {
        return _estimate[0];
      }

    private:
      double s;
  };

class G2O_TYPES_SBA_API EdgeTransformXYZo2XYZw : public  g2o::BaseMultiEdge<3, Vector3d> //, VertexSBAPointXYZ, VertexSE3Expmap, VertexScale>
{
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    EdgeTransformXYZo2XYZw(){resize(3);}

    bool read(std::istream& is);
    bool write(std::ostream& os) const;

    void computeError()  {
       const VertexSBAPointXYZ* Pw = static_cast<const VertexSBAPointXYZ*>(_vertices[0]);
       const VertexSE3Expmap* Tow = static_cast<const VertexSE3Expmap*>(_vertices[1]);
       //const VertexSE3Expmap* Tcw = static_cast<const VertexSE3Expmap*>(_vertices[2]);
       const VertexScale* s = static_cast<const VertexScale*>(_vertices[2]);
       //double scale = s->estimate()[0];
       Vector3d obs(_measurement);
       _error = obs -Tow->estimate().map(s->estimate()[0] * Pw->estimate());
      // std::cerr<<_error.transpose()<<std::endl;
    }

   // virtual void linearizeOplus();

};


} // end namespace

#endif
