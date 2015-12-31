#include "DetailSynthesis.h"
#include "Model.h"
#include "Shape.h"
#include "PolygonMesh.h"
#include "Bound.h"

#include "MeshParameterization.h"
#include "ParaShape.h"
#include "SynthesisTool.h"
#include "CurveGuidedVectorField.h"
#include "KevinVectorField.h"
#include "NormalTransfer.h"

#include "KDTreeWrapper.h"
#include "ShapeUtility.h"
#include "obj_writer.h"
#include "GLActor.h"
#include "YMLHandler.h"
#include "Ray.h"

#include <string>

using namespace LG;

DetailSynthesis::DetailSynthesis()
{
  resolution = 512;
  normalize_max = -1.0;
}

DetailSynthesis::~DetailSynthesis()
{

}

void DetailSynthesis::testMeshPara(std::shared_ptr<Model> model)
{
  /*cv::FileStorage fs(model->getDataPath() + "/displacement.xml", cv::FileStorage::READ);
  cv::Mat displacement_mat;
  fs["displacement"] >> displacement_mat;
  PolygonMesh poly_mesh;
  ShapeUtility::matToMesh(displacement_mat, poly_mesh, model);*/

  {
    cv::FileStorage fs1(model->getDataPath() + "/smoothed_output_height.xml", cv::FileStorage::READ);
    cv::Mat smoothed_output_height;
    fs1["smoothed_output_height"] >> smoothed_output_height;
    PolygonMesh smoothed_output_height_mesh;
    ShapeUtility::heightToMesh(smoothed_output_height, smoothed_output_height_mesh, model); // generate the displacement mesh

    cv::FileStorage fs2(model->getDataPath() + "/final_height.xml", cv::FileStorage::READ);
    cv::Mat final_height;
    fs2["final_height"] >> final_height;
    PolygonMesh final_height_mesh;
    ShapeUtility::heightToMesh(final_height, final_height_mesh, model); // generate the displacement mesh
  }

  mesh_para.reset(new MeshParameterization);

  mesh_para->doMeshParameterization(model);

  //mesh_para->shape_patches.clear();
  //mesh_para->shape_patches.resize(model->getPlaneFaces().size());
  //for (int i = 0; i < model->getPlaneFaces().size(); ++i)
  //{
  //  mesh_para->doMeshParamterizationPatch(model, i, &mesh_para->shape_patches[i]);
  //  mesh_para->shape_patches[i].initUVKDTree();
  //}
}


void DetailSynthesis::prepareFeatureMap(std::shared_ptr<Model> model)
{
  // compute feature
  // 1. normalized height
  // 2. surface normal 
  // 3. multi-scale solid angle curvature, not implemented yet
  // 4. directional occlusion

  ShapeUtility::computeSymmetry(model);
  ShapeUtility::computeNormalizedHeight(model);

  //ShapeUtility::computeDirectionalOcclusion(model);

  PolygonMesh* poly_mesh = model->getPolygonMesh();
  PolygonMesh::Vertex_attribute<Scalar> normalized_height = poly_mesh->vertex_attribute<Scalar>("v:NormalizedHeight");
  PolygonMesh::Vertex_attribute<STLVectorf> directional_occlusion = poly_mesh->vertex_attribute<STLVectorf>("v:DirectionalOcclusion");
  PolygonMesh::Vertex_attribute<Vec3> v_normals = poly_mesh->vertex_attribute<Vec3>("v:normal");
  //PolygonMesh::Vertex_attribute<Vec3> v_symmetry = poly_mesh->vertex_attribute<Vec3>("v:symmetry");
  PolygonMesh::Vertex_attribute<std::vector<float>> v_symmetry = poly_mesh->vertex_attribute<std::vector<float>>("v:symmetry");

  std::vector<std::vector<float> > vertex_feature_list(poly_mesh->n_vertices(), std::vector<float>());
  for (auto vit : poly_mesh->vertices())
  {
    vertex_feature_list[vit.idx()].push_back(normalized_height[vit]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][0]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][1]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][2]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][3]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][4]);
    vertex_feature_list[vit.idx()].push_back(v_normals[vit][0]);
    vertex_feature_list[vit.idx()].push_back(v_normals[vit][1]);
    vertex_feature_list[vit.idx()].push_back(v_normals[vit][2]);
    for (size_t i = 0; i < directional_occlusion[vit].size(); ++i)
    {
      vertex_feature_list[vit.idx()].push_back(directional_occlusion[vit][i]/directional_occlusion[vit].size());
    }
  }

  computeFeatureMap(mesh_para->seen_part.get(), vertex_feature_list);
  computeFeatureMap(mesh_para->unseen_part.get(), vertex_feature_list);
  //for (int i = 0; i < model->getPlaneFaces().size(); ++i)
  //{
  //  computeFeatureMap(&mesh_para->shape_patches[i], vertex_feature_list);
  //}

  // save feature map to file
  for (size_t i = 0; i < mesh_para->seen_part->feature_map.size(); ++i)
  {
    //YMLHandler::saveToFile(model->getOutputPath(), std::string("src_feature_") + std::to_string(i) + ".yml", src_feature_map[i]);
    //YMLHandler::saveToFile(model->getOutputPath(), std::string("tar_feature_") + std::to_string(i) + ".yml", tar_feature_map[i]);

    //YMLHandler::saveToMat(model->getOutputPath(), std::string("src_feature_") + std::to_string(i) + ".mat", src_feature_map[i]);
    //YMLHandler::saveToMat(model->getOutputPath(), std::string("tar_feature_") + std::to_string(i) + ".mat", tar_feature_map[i]);
  }
}

void DetailSynthesis::computeFeatureMap(ParaShape* para_shape, std::vector<std::vector<float> >& feature_list)
{
  //resolution = 512;
  int dim_feature = feature_list[0].size();
  para_shape->feature_map.clear();
  for(int i = 0; i < dim_feature; i ++)
  {
    para_shape->feature_map.push_back(cv::Mat(resolution, resolution, CV_32FC1));
  }
  /*feature_map = cv::Mat(resolution, resolution, CV_32FC3);*/
  /*int dims[3] = { resolution, resolution, 3};
  feature_map.create(3, dims, CV_32F);*/
  std::shared_ptr<Shape> shape = para_shape->cut_shape;
  std::shared_ptr<KDTreeWrapper> kdTree = para_shape->kdTree_UV;
  STLVectori v_set = para_shape->vertex_set;

  int f_id;
  std::vector<int> v_ids;
  std::vector<float> bary_coord;
  std::vector<float> pt(2, 0);
  for(int x = 0; x < resolution; x ++)
  {
    for(int y = 0; y < resolution; y ++)
    {
      bool inside_para = false;
      pt[0] = float(x) / resolution;
      pt[1] = float(y) / resolution;
      if (ShapeUtility::findClosestUVFace(pt, para_shape, bary_coord, f_id, v_ids))
      {
        inside_para = true;
      }

      // put feature into feature map from feature_list
      for (int i = 0; i < dim_feature; ++i)
      {
        if (inside_para)
        {
          para_shape->feature_map[i].at<float>(resolution - y - 1,x) = bary_coord[0] * feature_list[v_set[v_ids[0]]][i] + bary_coord[1] * feature_list[v_set[v_ids[1]]][i] + bary_coord[2] * feature_list[v_set[v_ids[2]]][i];
        }
        else
        {
          para_shape->feature_map[i].at<float>(resolution - y - 1,x) = 0.0;
        }
      }


      /*float v1_normal_original_mesh[3],v2_normal_original_mesh[3],v3_normal_original_mesh[3];
      v1_normal_original_mesh[0] = mesh_para->normal_original_mesh[3 * v_set[id1]];
      v1_normal_original_mesh[1] = mesh_para->normal_original_mesh[3 * v_set[id1] + 1];
      v1_normal_original_mesh[2] = mesh_para->normal_original_mesh[3 * v_set[id1] + 2];
      v2_normal_original_mesh[0] = mesh_para->normal_original_mesh[3 * v_set[id2]];
      v2_normal_original_mesh[1] = mesh_para->normal_original_mesh[3 * v_set[id2] + 1];
      v2_normal_original_mesh[2] = mesh_para->normal_original_mesh[3 * v_set[id2] + 2];
      v3_normal_original_mesh[0] = mesh_para->normal_original_mesh[3 * v_set[id3]];
      v3_normal_original_mesh[1] = mesh_para->normal_original_mesh[3 * v_set[id3] + 1];
      v3_normal_original_mesh[2] = mesh_para->normal_original_mesh[3 * v_set[id3] + 2];*/

      /*feature_map.at<float>(x,y,0) = lambda[0] * v1_normal_original_mesh[0] + lambda[1] * v2_normal_original_mesh[0] + lambda[2] * v3_normal_original_mesh[0];
      feature_map.at<float>(x,y,1) = lambda[0] * v1_normal_original_mesh[1] + lambda[1] * v2_normal_original_mesh[1] + lambda[2] * v3_normal_original_mesh[1];
      feature_map.at<float>(x,y,2) = lambda[0] * v1_normal_original_mesh[2] + lambda[1] * v2_normal_original_mesh[2] + lambda[2] * v3_normal_original_mesh[2];*/
      /*feature_map.at<cv::Vec3f>(x,y)[0] = lambda[0] * v1_normal_original_mesh[0] + lambda[1] * v2_normal_original_mesh[0] + lambda[2] * v3_normal_original_mesh[0];
      feature_map.at<cv::Vec3f>(x,y)[1] = lambda[0] * v1_normal_original_mesh[1] + lambda[1] * v2_normal_original_mesh[1] + lambda[2] * v3_normal_original_mesh[1];
      feature_map.at<cv::Vec3f>(x,y)[2] = lambda[0] * v1_normal_original_mesh[2] + lambda[1] * v2_normal_original_mesh[2] + lambda[2] * v3_normal_original_mesh[2];*/
      /*feature_map[0].at<float>(x,y) = lambda[0] * v1_normal_original_mesh[0] + lambda[1] * v2_normal_original_mesh[0] + lambda[2] * v3_normal_original_mesh[0];
      feature_map[1].at<float>(x,y) = lambda[0] * v1_normal_original_mesh[1] + lambda[1] * v2_normal_original_mesh[1] + lambda[2] * v3_normal_original_mesh[1];
      feature_map[2].at<float>(x,y) = lambda[0] * v1_normal_original_mesh[2] + lambda[1] * v2_normal_original_mesh[2] + lambda[2] * v3_normal_original_mesh[2];*/
    }
  }

  std::cout << "feature min max: " << std::endl;
  for (size_t i = 0; i < para_shape->feature_map.size(); ++i)
  {
    double min, max;
    cv::minMaxLoc(para_shape->feature_map[i],&min,&max);
    std::cout << min << " " << max << " ";
  }
  std::cout << std::endl;
}

void DetailSynthesis::prepareDetailMap(std::shared_ptr<Model> model)
{
  // Prepare the detail map for synthesis
  // 1. details include displacement and reflectance, both are cv::Mat ?
  // 2. need to convert them to the parameterized domain
  //   1) for each pixel in parameterized image, found its original position by barycentric interpolation
  //   2) project it onto image plane and take corresponding value in original details image
  //   3) store the value into the parameterized image

  cv::FileStorage fs(model->getDataPath() + "/reflectance.xml", cv::FileStorage::READ);
  cv::Mat detail_reflectance_mat;
  fs["reflectance"] >> detail_reflectance_mat;

  cv::FileStorage fs2(model->getDataPath() + "/displacement.xml", cv::FileStorage::READ);
  cv::Mat displacement_mat;
  fs2["displacement"] >> displacement_mat;
  //{
  //  double n_disp_min, n_disp_max;
  //  cv::minMaxLoc(displacement_mat, &n_disp_min, &n_disp_max);
  //  displacement_mat = (displacement_mat - n_disp_min) / (n_disp_max - n_disp_min);
  //}

  std::vector<cv::Mat> detail_image(3);
  cv::split(detail_reflectance_mat, &detail_image[0]);
  {
    // dilate the detail map in case of black
    for (int i = 0; i < 3; ++i)
    {
      ShapeUtility::dilateImage(detail_image[i], 15);
    }
    //cv::merge(detail_image, detail_reflectance_mat);
    //double n_max, n_min;
    //cv::minMaxLoc(detail_reflectance_mat, &n_min, &n_max);
    //detail_reflectance_mat = detail_reflectance_mat / n_max;
  }
  std::cout<<"test"<< std::endl;
  std::vector<cv::Mat> new_detail_image;
  new_detail_image.push_back(detail_image[0]);
  new_detail_image.push_back(detail_image[1]);
  new_detail_image.push_back(detail_image[2]);
  new_detail_image.push_back(displacement_mat);

  //cv::merge(&detail_image[0], 3, detail_reflectance_mat);
  //imshow("dilate souroce", detail_reflectance_mat);


  computeDetailMap(mesh_para->seen_part.get(), new_detail_image, model, mesh_para->seen_part->cut_faces);std::cout<<"test2"<< std::endl;
  computeDetailMap(mesh_para->unseen_part.get(), new_detail_image, model, mesh_para->seen_part->cut_faces);std::cout<<"test3"<< std::endl;


  //for (int i = 0; i < model->getPlaneFaces().size(); ++i)
  //{
  //  computeDetailMap(&mesh_para->shape_patches[i], detail_image, model, mesh_para->seen_part->cut_faces);
  //}

  
  //PolygonMesh displacement_mesh;
  //ShapeUtility::matToMesh(displacement_mat, displacement_mesh, model); // generate the displacement mesh
  //computeDisplacementMap(mesh_para->seen_part.get(), &displacement_mesh, model, mesh_para->seen_part->cut_faces);
  //computeDisplacementMap(mesh_para->unseen_part.get(), &displacement_mesh, model, mesh_para->seen_part->cut_faces);

  //YMLHandler::saveToFile(model->getOutputPath(), std::string("displacement_map") + ".yml", displacement_map);
  //YMLHandler::saveToMat(model->getOutputPath(), std::string("displacement_map") + ".mat", displacement_map);

  // save detail map to file
  for (size_t i = 0; i < mesh_para->seen_part->detail_map.size(); ++i)
  {
    //YMLHandler::saveToFile(model->getOutputPath(), std::string("src_detail_") + std::to_string(i) + ".yml", src_detail_map[i]);
    //YMLHandler::saveToMat(model->getOutputPath(), std::string("src_detail_") + std::to_string(i) + ".mat", src_detail_map[i]);
  }
}

void DetailSynthesis::computeDisplacementMap(ParaShape* para_shape, PolygonMesh* displacement_mesh, std::shared_ptr<Model> model, std::set<int>& visible_faces)
{
  VertexList vertex_list;
  vertex_list.clear();
  for (auto vit : displacement_mesh->vertices())
  {
    const Vec3& pt = displacement_mesh->position(vit);
    vertex_list.push_back(pt[0]);
    vertex_list.push_back(pt[1]);
    vertex_list.push_back(pt[2]);
  }
  FaceList face_list;
  face_list.clear();
  for (auto fit : displacement_mesh->faces())
  {
    for (auto vfc_it : displacement_mesh->vertices(fit))
    {
      face_list.push_back(vfc_it.idx());
    }
  } 
  Ray ray_instance;
  ray_instance.passModel(vertex_list, face_list); // initialize the displacement mesh ray

  //resolution = 512;
  cv::Mat displacement_map(resolution, resolution, CV_32FC1);

  PolygonMesh* poly_mesh = model->getPolygonMesh();
  std::shared_ptr<Shape> shape = para_shape->cut_shape;
  std::shared_ptr<KDTreeWrapper> kdTree = para_shape->kdTree_UV;
  STLVectori v_set = para_shape->vertex_set;
  PolygonMesh::Vertex_attribute<Vec3> v_normals = poly_mesh->vertex_attribute<Vec3>("v:normal");
  
  AdjList adjFaces_list = shape->getVertexShareFaces();
  std::vector<float> pt(2, 0);
  for(int x = 0; x < resolution; x ++)
  {
    for(int y = 0; y < resolution; y ++)
    {
      //int pt_id;
      //std::vector<float> pt;
      //pt.resize(2);
      //pt[0] = float(x) / resolution;
      //pt[1] = float(y) / resolution;
      //kdTree->nearestPt(pt,pt_id); // pt has been modified
      //std::vector<int> adjFaces = adjFaces_list[pt_id];
      //int face_id;
      //float point[3];
      //point[0] = float(x) / resolution;//pt[0];
      //point[1] = float(y) / resolution;;//pt[1];
      //point[2] = 0;
      //float lambda[3];
      //int id1,id2,id3;
      //for(size_t i = 0; i < adjFaces.size(); i ++)
      //{
      //  float l[3];
      //  int v1_id,v2_id,v3_id;
      //  float v1[3],v2[3],v3[3];
      //  v1_id = (shape->getFaceList())[3 * adjFaces[i]];
      //  v2_id = (shape->getFaceList())[3 * adjFaces[i] + 1];
      //  v3_id = (shape->getFaceList())[3 * adjFaces[i] + 2];
      //  v1[0] = (shape->getUVCoord())[2 * v1_id];
      //  v1[1] = (shape->getUVCoord())[2 * v1_id + 1];
      //  v1[2] = 0;
      //  v2[0] = (shape->getUVCoord())[2 * v2_id];
      //  v2[1] = (shape->getUVCoord())[2 * v2_id + 1];
      //  v2[2] = 0;
      //  v3[0] = (shape->getUVCoord())[2 * v3_id];
      //  v3[1] = (shape->getUVCoord())[2 * v3_id + 1];
      //  v3[2] = 0;
      //  ShapeUtility::computeBaryCentreCoord(point,v1,v2,v3,l);
      //  l[0] = (fabs(l[0]) < 1e-4) ? 0 : l[0];
      //  l[1] = (fabs(l[1]) < 1e-4) ? 0 : l[1];
      //  l[2] = (fabs(l[2]) < 1e-4) ? 0 : l[2];
      //  if(l[0] >= 0 && l[1] >= 0 && l[2] >= 0)
      //  {
      //    face_id = i;
      //    lambda[0] = l[0];
      //    lambda[1] = l[1];
      //    lambda[2] = l[2];
      //    id1 = v1_id;
      //    id2 = v2_id;
      //    id3 = v3_id;
      //  }
      //}
      pt[0] = float(x) / resolution;
      pt[1] = float(y) / resolution;
      int face_id;
      std::vector<int> id;
      std::vector<float> lambda;
      //ShapeUtility::findFaceId(x, y, resolution, kdTree, adjFaces_list, shape, face_id, lambda, id1, id2, id3);
      if(ShapeUtility::findClosestUVFace(pt, para_shape, lambda, face_id, id))
      {
        if (visible_faces.find(para_shape->face_set[face_id]) != visible_faces.end())
        {
          Vector3f pos = lambda[0] * poly_mesh->position(PolygonMesh::Vertex(v_set[id[0]]))
                       + lambda[1] * poly_mesh->position(PolygonMesh::Vertex(v_set[id[1]]))
                       + lambda[2] * poly_mesh->position(PolygonMesh::Vertex(v_set[id[2]]));
          Vector3f dir = lambda[0] * v_normals[PolygonMesh::Vertex(v_set[id[0]])]
                       + lambda[1] * v_normals[PolygonMesh::Vertex(v_set[id[1]])]
                       + lambda[2] * v_normals[PolygonMesh::Vertex(v_set[id[2]])];

          dir.normalized();
          Vector3f end = pos + model->getBoundBox()->getRadius() * dir / 10;
          Vector3f neg_end = pos - model->getBoundBox()->getRadius() * dir / 10;
          double intersect_point[3];
          double neg_intersect_point[3];
          Eigen::Vector3d start_pt, end_pt, neg_end_pt, epslon;
          start_pt << pos(0), pos(1), pos(2);
          end_pt << end(0), end(1), end(2);
          neg_end_pt << neg_end(0), neg_end(1), neg_end(2);
          epslon << 1e-5, 1e-5, 1e-5;
          bool is_intersected, is_neg_intersected;
          is_intersected = ray_instance.intersectModel(start_pt + epslon, end_pt, intersect_point);
          is_neg_intersected = ray_instance.intersectModel(start_pt - epslon, neg_end_pt, neg_intersect_point);
          if(is_intersected == false && is_neg_intersected == false)
          {
            displacement_map.at<float>(resolution - y - 1,x) = 0;
          }
          else
          {
            float distance;
            float neg_distance;
            distance = sqrt((intersect_point[0] - pos(0)) * (intersect_point[0] - pos(0)) 
                          + (intersect_point[1] - pos(1)) * (intersect_point[1] - pos(1)) 
                          + (intersect_point[2] - pos(2)) * (intersect_point[2] - pos(2)));
            neg_distance = sqrt((neg_intersect_point[0] - pos(0)) * (neg_intersect_point[0] - pos(0)) 
                          + (neg_intersect_point[1] - pos(1)) * (neg_intersect_point[1] - pos(1)) 
                          + (neg_intersect_point[2] - pos(2)) * (neg_intersect_point[2] - pos(2)));
            displacement_map.at<float>(resolution - y - 1,x) = distance > neg_distance ? -neg_distance : distance;
          }
        }
        else
        {
          displacement_map.at<float>(resolution - y - 1,x) = -1;
        }
      }
      else
      {
        displacement_map.at<float>(resolution - y - 1,x) = -1;
      }
    }
  }

  para_shape->detail_map.push_back(displacement_map);
  //applyDisplacementMap(mesh_para->seen_part->vertex_set, mesh_para->seen_part->cut_shape, model, displacement_map);
}

void DetailSynthesis::computeDetailMap(ParaShape* para_shape, std::vector<cv::Mat>& detail_image, std::shared_ptr<Model> model, std::set<int>& visible_faces)
{
  //resolution = 512;
  int n_filled_pixel = 0;
  int dim_detail = detail_image.size();
  para_shape->detail_map.clear();
  for(int i = 0; i < dim_detail; i ++)
  {
    para_shape->detail_map.push_back(cv::Mat(resolution, resolution, CV_32FC1));
  }

  PolygonMesh* poly_mesh = model->getPolygonMesh();
  std::shared_ptr<Shape> shape = para_shape->cut_shape;
  std::shared_ptr<KDTreeWrapper> kdTree = para_shape->kdTree_UV;
  STLVectori v_set = para_shape->vertex_set;
  
  AdjList adjFaces_list = shape->getVertexShareFaces();
  std::vector<float> pt(2, 0);
  for(int x = 0; x < resolution; x ++)
  {
    for(int y = 0; y < resolution; y ++)
    {
      //int pt_id;
      //std::vector<float> pt;
      //pt.resize(2);
      //pt[0] = float(x) / resolution;
      //pt[1] = float(y) / resolution;
      //kdTree->nearestPt(pt,pt_id); // pt has been modified
      //std::vector<int> adjFaces = adjFaces_list[pt_id];
      //int face_id;
      //float point[3];
      //point[0] = float(x) / resolution;//pt[0];
      //point[1] = float(y) / resolution;;//pt[1];
      //point[2] = 0;
      //float lambda[3];
      //int id1,id2,id3;
      //for(size_t i = 0; i < adjFaces.size(); i ++)
      //{
      //  float l[3];
      //  int v1_id,v2_id,v3_id;
      //  float v1[3],v2[3],v3[3];
      //  v1_id = (shape->getFaceList())[3 * adjFaces[i]];
      //  v2_id = (shape->getFaceList())[3 * adjFaces[i] + 1];
      //  v3_id = (shape->getFaceList())[3 * adjFaces[i] + 2];
      //  v1[0] = (shape->getUVCoord())[2 * v1_id];
      //  v1[1] = (shape->getUVCoord())[2 * v1_id + 1];
      //  v1[2] = 0;
      //  v2[0] = (shape->getUVCoord())[2 * v2_id];
      //  v2[1] = (shape->getUVCoord())[2 * v2_id + 1];
      //  v2[2] = 0;
      //  v3[0] = (shape->getUVCoord())[2 * v3_id];
      //  v3[1] = (shape->getUVCoord())[2 * v3_id + 1];
      //  v3[2] = 0;
      //  ShapeUtility::computeBaryCentreCoord(point,v1,v2,v3,l);
      //  l[0] = (fabs(l[0]) < 1e-4) ? 0 : l[0];
      //  l[1] = (fabs(l[1]) < 1e-4) ? 0 : l[1];
      //  l[2] = (fabs(l[2]) < 1e-4) ? 0 : l[2];
      //  if(l[0] >= 0 && l[1] >= 0 && l[2] >= 0)
      //  {
      //    face_id = adjFaces[i];
      //    lambda[0] = l[0];
      //    lambda[1] = l[1];
      //    lambda[2] = l[2];
      //    id1 = v1_id;
      //    id2 = v2_id;
      //    id3 = v3_id;
      //  }
      //}
      pt[0] = float(x) / resolution;
      pt[1] = float(y) / resolution;
      int face_id;
      std::vector<int> id;
      std::vector<float> lambda;
      if(ShapeUtility::findClosestUVFace(pt, para_shape, lambda, face_id, id))
      {

        //ShapeUtility::findFaceId(x, y, resolution, kdTree, adjFaces_list, shape, face_id, lambda, id1, id2, id3);
        // first it needs to be in the visible face, face_id need to be mapped to original face id
        if (visible_faces.find(para_shape->face_set[face_id]) != visible_faces.end())
        {
          Vector3f pos = lambda[0] * poly_mesh->position(PolygonMesh::Vertex(v_set[id[0]]))
            + lambda[1] * poly_mesh->position(PolygonMesh::Vertex(v_set[id[1]]))
            + lambda[2] * poly_mesh->position(PolygonMesh::Vertex(v_set[id[2]]));
          float winx, winy;//std::cout<< pos.transpose() << " ";
          model->getProjectPt(pos.data(), winx, winy); // start from left upper corner
          winy = winy < 0 ? 0 : (winy >= detail_image[0].rows ? detail_image[0].rows - 1 : winy);
          winx = winx < 0 ? 0 : (winx >= detail_image[0].cols ? detail_image[0].cols - 1 : winx);
          // put detail into detail map from detail image
          for (int i = 0; i < dim_detail; ++i)
          {
            para_shape->detail_map[i].at<float>(resolution - y - 1,x) = detail_image[i].at<float>(winy, winx);
          }

          ++n_filled_pixel;
        }
        else
        {
          for (int i = 0; i < dim_detail; ++i)
          {
            para_shape->detail_map[i].at<float>(resolution - y - 1,x) = -1.0;
          }
        }
      }
      else
      {
        for (int i = 0; i < dim_detail; ++i)
        {
          para_shape->detail_map[i].at<float>(resolution - y - 1,x) = -1.0;
        }
      }
    }
  }

  // record the fill ratio and tag whether it is full filled
  if (n_filled_pixel == (resolution * resolution))
  {
    para_shape->filled = 1;
    para_shape->fill_ratio = 1.0;
  }
  else
  {
    para_shape->filled = 0;
    para_shape->fill_ratio = float(n_filled_pixel) / (resolution * resolution);
  }

  std::cout << "detail min max: " << std::endl;
  for (size_t i = 0; i < para_shape->detail_map.size(); ++i)
  {
    double min, max;
    cv::minMaxLoc(para_shape->detail_map[i],&min,&max);
    std::cout << min << " " << max << " ";
  }
  std::cout << std::endl;
}

//old one, useless 
void DetailSynthesis::computeDisplacementMap(std::shared_ptr<Model> model)
{
  //resolution = 500;
  //displacement_map = cv::Mat(resolution, resolution, CV_32FC1);
  //std::shared_ptr<KDTreeWrapper> kdTree = mesh_para->seen_part->kdTree_UV;
  //AdjList adjFaces_list = mesh_para->seen_part->cut_shape->getVertexShareFaces();
  //for(int x = 0; x < resolution; x ++)
  //{
  //  for(int y = 0; y < resolution; y ++)
  //  {
  //    int pt_id;
  //    std::vector<float> pt;
  //    pt.resize(2);
  //    pt[0] = float(x) / resolution;
  //    pt[1] = float(y) / resolution;
  //    kdTree->nearestPt(pt,pt_id);
  //    std::vector<int> adjFaces = adjFaces_list[pt_id];
  //    int face_id;
  //    float point[3];
  //    point[0] = pt[0];
  //    point[1] = pt[1];
  //    point[2] = 0;
  //    float lambda[3];
  //    int id1_before,id2_before,id3_before;
  //    for(size_t i = 0; i < adjFaces.size(); i ++)
  //    {
  //      float l[3];
  //      int v1_id,v2_id,v3_id;
  //      float v1[3],v2[3],v3[3];
  //      v1_id = (mesh_para->seen_part->cut_shape->getFaceList())[3 * adjFaces[i]];
  //      v2_id = (mesh_para->seen_part->cut_shape->getFaceList())[3 * adjFaces[i] + 1];
  //      v3_id = (mesh_para->seen_part->cut_shape->getFaceList())[3 * adjFaces[i] + 2];
  //      v1[0] = (mesh_para->seen_part->cut_shape->getUVCoord())[2 * v1_id];
  //      v1[1] = (mesh_para->seen_part->cut_shape->getUVCoord())[2 * v1_id + 1];
  //      v1[2] = 0;
  //      v2[0] = (mesh_para->seen_part->cut_shape->getUVCoord())[2 * v2_id];
  //      v2[1] = (mesh_para->seen_part->cut_shape->getUVCoord())[2 * v2_id + 1];
  //      v2[2] = 0;
  //      v3[0] = (mesh_para->seen_part->cut_shape->getUVCoord())[2 * v3_id];
  //      v3[1] = (mesh_para->seen_part->cut_shape->getUVCoord())[2 * v3_id + 1];
  //      v3[2] = 0;
  //      ShapeUtility::computeBaryCentreCoord(point,v1,v2,v3,l);
  //      if(l[0] >= 0 && l[1] >= 0 && l[2] >= 0)
  //      {
  //        face_id = i;
  //        lambda[0] = l[0];
  //        lambda[1] = l[1];
  //        lambda[2] = l[2];
  //        id1_before = v1_id;
  //        id2_before = v2_id;
  //        id3_before = v3_id;
  //      }
  //    }
  //    float v1_worldCoord_before[3],v2_worldCoord_before[3],v3_worldCoord_before[3];
  //    v1_worldCoord_before[0] = (mesh_para->cut_shape->getVertexList())[3 * id1_before];
  //    v1_worldCoord_before[1] = (mesh_para->cut_shape->getVertexList())[3 * id1_before + 1];
  //    v1_worldCoord_before[2] = (mesh_para->cut_shape->getVertexList())[3 * id1_before + 2];
  //    v2_worldCoord_before[0] = (mesh_para->cut_shape->getVertexList())[3 * id2_before];
  //    v2_worldCoord_before[1] = (mesh_para->cut_shape->getVertexList())[3 * id2_before + 1];
  //    v2_worldCoord_before[2] = (mesh_para->cut_shape->getVertexList())[3 * id2_before + 2];
  //    v3_worldCoord_before[0] = (mesh_para->cut_shape->getVertexList())[3 * id3_before];
  //    v3_worldCoord_before[1] = (mesh_para->cut_shape->getVertexList())[3 * id3_before + 1];
  //    v3_worldCoord_before[2] = (mesh_para->cut_shape->getVertexList())[3 * id3_before + 2];
  //    float pt_worldCoord_before[3];
  //    pt_worldCoord_before[0] = lambda[0] * v1_worldCoord_before[0] + lambda[1] * v2_worldCoord_before[0] + lambda[2] * v3_worldCoord_before[0];
  //    pt_worldCoord_before[1] = lambda[0] * v1_worldCoord_before[1] + lambda[1] * v2_worldCoord_before[1] + lambda[2] * v3_worldCoord_before[1];
  //    pt_worldCoord_before[2] = lambda[0] * v1_worldCoord_before[2] + lambda[1] * v2_worldCoord_before[2] + lambda[2] * v3_worldCoord_before[2];
  //    int id1_after,id2_after,id3_after;
  //    id1_after = mesh_para->vertex_set[id1_before];
  //    id2_after = mesh_para->vertex_set[id2_before];
  //    id3_after = mesh_para->vertex_set[id3_before];
  //    float v1_worldCoord_after[3],v2_worldCoord_after[3],v3_worldCoord_after[3];
  //    v1_worldCoord_after[0] = (model->getShapeVertexList())[3 * id1_after];
  //    v1_worldCoord_after[1] = (model->getShapeVertexList())[3 * id1_after + 1];
  //    v1_worldCoord_after[2] = (model->getShapeVertexList())[3 * id1_after + 2];
  //    v2_worldCoord_after[0] = (model->getShapeVertexList())[3 * id2_after];
  //    v2_worldCoord_after[1] = (model->getShapeVertexList())[3 * id2_after + 1];
  //    v2_worldCoord_after[2] = (model->getShapeVertexList())[3 * id2_after + 2];
  //    v3_worldCoord_after[0] = (model->getShapeVertexList())[3 * id3_after];
  //    v3_worldCoord_after[1] = (model->getShapeVertexList())[3 * id3_after + 1];
  //    v3_worldCoord_after[2] = (model->getShapeVertexList())[3 * id3_after + 2];
  //    float pt_worldCoord_after[3];
  //    pt_worldCoord_after[0] = lambda[0] * v1_worldCoord_after[0] + lambda[1] * v2_worldCoord_after[0] + lambda[2] * v3_worldCoord_after[0];
  //    pt_worldCoord_after[1] = lambda[0] * v1_worldCoord_after[1] + lambda[1] * v2_worldCoord_after[1] + lambda[2] * v3_worldCoord_after[1];
  //    pt_worldCoord_after[2] = lambda[0] * v1_worldCoord_after[2] + lambda[1] * v2_worldCoord_after[2] + lambda[2] * v3_worldCoord_after[2];
  //    float v1_normal_original_mesh[3],v2_normal_original_mesh[3],v3_normal_original_mesh[3];
  //    v1_normal_original_mesh[0] = mesh_para->normal_original_mesh[3 * id1_after];
  //    v1_normal_original_mesh[1] = mesh_para->normal_original_mesh[3 * id1_after + 1];
  //    v1_normal_original_mesh[2] = mesh_para->normal_original_mesh[3 * id1_after + 2];
  //    v2_normal_original_mesh[0] = mesh_para->normal_original_mesh[3 * id2_after];
  //    v2_normal_original_mesh[1] = mesh_para->normal_original_mesh[3 * id2_after + 1];
  //    v2_normal_original_mesh[2] = mesh_para->normal_original_mesh[3 * id2_after + 2];
  //    v3_normal_original_mesh[0] = mesh_para->normal_original_mesh[3 * id3_after];
  //    v3_normal_original_mesh[1] = mesh_para->normal_original_mesh[3 * id3_after + 1];
  //    v3_normal_original_mesh[2] = mesh_para->normal_original_mesh[3 * id3_after + 2];
  //    float pt_normal_original_mesh[3];
  //    pt_normal_original_mesh[0] = lambda[0] * v1_normal_original_mesh[0] + lambda[1] * v2_normal_original_mesh[0] + lambda[2] * v3_normal_original_mesh[0];
  //    pt_normal_original_mesh[1] = lambda[0] * v1_normal_original_mesh[1] + lambda[1] * v2_normal_original_mesh[1] + lambda[2] * v3_normal_original_mesh[1];
  //    pt_normal_original_mesh[2] = lambda[0] * v1_normal_original_mesh[2] + lambda[1] * v2_normal_original_mesh[2] + lambda[2] * v3_normal_original_mesh[2];
  //    float pt_difference[3];
  //    pt_difference[0] = pt_worldCoord_after[0] - pt_worldCoord_before[0];
  //    pt_difference[1] = pt_worldCoord_after[1] - pt_worldCoord_before[1];
  //    pt_difference[2] = pt_worldCoord_after[2] - pt_worldCoord_before[2];
  //    displacement_map.at<float>(x,y) = pt_difference[0] * pt_normal_original_mesh[0] + pt_difference[1] * pt_normal_original_mesh[1] + pt_difference[2] * pt_normal_original_mesh[2];
  //  }
  //}
  //// show the displacement_map
  ///*double min,max;
  //cv::minMaxLoc(displacement_map,&min,&max);
  //cv::imshow("displacement_map",(displacement_map - min) / (max - min));*/
  //applyDisplacementMap(mesh_para->vertex_set, mesh_para->cut_shape, model, displacement_map);
}

void DetailSynthesis::applyDisplacementMap(STLVectori vertex_set, std::shared_ptr<Shape> cut_shape, std::shared_ptr<Model> model, cv::Mat disp_map)
{
  VertexList vertex_check;
  //vertex_check =  mesh_para->vertex_original_mesh;
  vertex_check = model->getShapeVertexList();
  const NormalList& ori_normal = model->getShapeNormalList();
  for(int i = 0; i < vertex_set.size(); i ++)
  {
    float pt_check[3];
    pt_check[0] = vertex_check[3 * vertex_set[i]];
    pt_check[1] = vertex_check[3 * vertex_set[i] + 1];
    pt_check[2] = vertex_check[3 * vertex_set[i] + 2];
    float normal_check[3];
    normal_check[0] = ori_normal[3 * vertex_set[i]];
    normal_check[1] = ori_normal[3 * vertex_set[i] + 1];
    normal_check[2] = ori_normal[3 * vertex_set[i] + 2];
    float displacement;
    float U,V;
    U = (cut_shape->getUVCoord())[2 * i];
    V = (cut_shape->getUVCoord())[2 * i + 1];
    //std::cout << "U : " << U << " , " << "V : " << V << std::endl;
    int img_x,img_y;
    img_x = int(U * (float)resolution);
    img_y = int(V * (float)resolution);
    if(img_x == resolution)
    {
      img_x --;
    }
    if(img_y == resolution)
    {
      img_y --;    
    }
    displacement = disp_map.at<float>(resolution - img_y - 1, img_x);
    vertex_check[3 * vertex_set[i]] = pt_check[0] + normal_check[0] * displacement * 1.0;
    vertex_check[3 * vertex_set[i] + 1] = pt_check[1] + normal_check[1] * displacement * 1.0;
    vertex_check[3 * vertex_set[i] + 2] = pt_check[2] + normal_check[2] * displacement * 1.0;
  }

  model->updateShape(vertex_check);

  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  tinyobj::shape_t obj_shape;
  obj_shape.mesh.positions = vertex_check;
  obj_shape.mesh.indices = model->getShapeFaceList();
  //obj_shape.mesh.texcoords = model->getShapeUVCoord();
  shapes.push_back(obj_shape);

  char time_postfix[50];
  time_t current_time = time(NULL);
  strftime(time_postfix, sizeof(time_postfix), "_%Y%m%d-%H%M%S", localtime(&current_time));
  std::string file_time_postfix = time_postfix;

  std::string output_name = model->getOutputPath() + "/detail_synthesis" + file_time_postfix + ".obj";
  WriteObj(output_name, shapes, materials);
}


void DetailSynthesis::startDetailSynthesis(std::shared_ptr<Model> model)
{
  //this->prepareDetailMap(model);
  //this->prepareFeatureMap(model);

  //this->patchSynthesis(model);
  //this->mergeSynthesis(mesh_para->unseen_part.get(), model);

  //this->patchSynthesis(model);
  //this->mergeSynthesis(model);

  //cv::Mat load_img = cv::imread(model->getDataPath() + "/syntext/0.9-.png");
  //cv::Mat syn_ref_ext;
  //if (load_img.data != NULL)
  //{
  //  load_img.convertTo(syn_ref_ext, CV_32FC3);
  //  syn_ref_ext = syn_ref_ext / 255.0;
  //}
  //src_detail_map.clear();
  //src_detail_map.resize(3);
  //cv::split(syn_ref_ext, &src_detail_map[0]);
  //src_feature_map.clear();
  //src_feature_map.resize(1, cv::Mat::zeros(src_detail_map[0].rows, src_detail_map[0].cols, CV_32FC1));
  //tar_feature_map = src_feature_map;

  //syn_tool.reset(new SynthesisTool);
  //syn_tool->init(mesh_para->seen_part->feature_map, mesh_para->unseen_part->feature_map, mesh_para->seen_part->detail_map);
  //syn_tool->doSynthesisNew();
  //syn_tool->doSynthesisWithMask(mesh_para->seen_part->feature_map, mesh_para->unseen_part->feature_map, mesh_para->seen_part->detail_map, mesh_para->unseen_part->detail_map);
  //syn_tool->doFilling(mesh_para->shape_patches[0].feature_map, mesh_para->shape_patches[0].detail_map);

  // map the synthesis to model color
  //resolution = 512;
  //std::vector<std::vector<cv::Mat> >& detail_result = syn_tool->getTargetDetail();
  //mesh_para->unseen_part->detail_map.clear();
  //mesh_para->unseen_part->detail_map.push_back(detail_result[0][0].clone());
  //mesh_para->unseen_part->detail_map.push_back(detail_result[1][0].clone());
  //mesh_para->unseen_part->detail_map.push_back(detail_result[2][0].clone());
  //mesh_para->unseen_part->detail_map.push_back(detail_result[3][0].clone());
  //detail_result[0][0]; // R
  //detail_result[1][0]; // G
  //detail_result[2][0]; // B
  //cv::Mat tar_displacement_map = detail_result[3][0];

  //cv::imshow("souorce feature", mesh_para->seen_part->feature_map[0]);
  //cv::imshow("souorce detail", mesh_para->seen_part->detail_map[3]);
  //cv::imshow("target feature", mesh_para->unseen_part->feature_map[0]);
  //cv::imshow("target feature", mesh_para->unseen_part->detail_map[3]);

  //applyDisplacementMap(mesh_para->seen_part->vertex_set, mesh_para->seen_part->cut_shape, model, mesh_para->seen_part->detail_map[3]);
  //STLVectori intersect_seen_unseen; // we don't want to the crossed region move twice
  //std::set_intersection(mesh_para->unseen_part->vertex_set.begin(), mesh_para->unseen_part->vertex_set.end(),
  //  mesh_para->seen_part->vertex_set.begin(), mesh_para->seen_part->vertex_set.end(),
  //  std::inserter(intersect_seen_unseen, intersect_seen_unseen.begin()));
  //STLVectori real_unseen_vertex_set;
  //std::set_difference(mesh_para->unseen_part->vertex_set.begin(), mesh_para->unseen_part->vertex_set.end(),
  //  intersect_seen_unseen.begin(), intersect_seen_unseen.end(), std::inserter(real_unseen_vertex_set, real_unseen_vertex_set.begin()));
  //applyDisplacementMap(real_unseen_vertex_set, mesh_para->unseen_part->cut_shape, model, mesh_para->unseen_part->detail_map[3]);
  //cv::Mat load_img = cv::imread(model->getDataPath() + "/syntext/0.9-.png");
  //cv::Mat syn_ref_ext;
  //if (load_img.data != NULL)
  //{
  //  load_img.convertTo(syn_ref_ext, CV_32FC3);
  //  syn_ref_ext = syn_ref_ext / 255.0;
  //}
  //std::vector<cv::Mat> output_detail_ext(3);
  //cv::split(syn_ref_ext, &output_detail_ext[0]);
  //std::vector<std::vector<cv::Mat> > detail_result(3);
  //detail_result[0].push_back(output_detail_ext[2].clone());
  //detail_result[1].push_back(output_detail_ext[1].clone());
  //detail_result[2].push_back(output_detail_ext[0].clone());

  //cv::Mat tar_detail_last_level;
  //std::vector<cv::Mat> output_detail_last_level;
  //output_detail_last_level.push_back(detail_result[2][1].clone());
  //output_detail_last_level.push_back(detail_result[1][1].clone());
  //output_detail_last_level.push_back(detail_result[0][1].clone());
  //cv::merge(&output_detail_last_level[0], 3, tar_detail_last_level);
  //cv::imshow("last level", tar_detail_last_level);


  PolygonMesh* poly_mesh = model->getPolygonMesh();
  PolygonMesh::Vertex_attribute<int> syn_texture_tag = poly_mesh->vertex_attribute<int>("v:syn_texture_tag");
  PolygonMesh::Vertex_attribute<Vec2> hidden_uv_list = poly_mesh->vertex_attribute<Vec2>("v:hidden_uv");

  cv::Mat tar_detail;
  std::vector<cv::Mat> output_detail;
  output_detail.push_back(mesh_para->unseen_part->detail_map[2].clone());
  output_detail.push_back(mesh_para->unseen_part->detail_map[1].clone());
  output_detail.push_back(mesh_para->unseen_part->detail_map[0].clone());
  cv::merge(&output_detail[0], 3, tar_detail);
  double min,max;
  cv::minMaxLoc(tar_detail,&min,&normalize_max);
  normalize_max = 1;
  tar_detail = tar_detail / normalize_max;
  model->getSynRImg() = tar_detail.clone();
  cv::imshow("result", (tar_detail));

  // store the detail map result to unseen part
  //mesh_para->unseen_part->detail_map.clear();
  //mesh_para->unseen_part->detail_map.push_back(detail_result[0][0].clone());
  //mesh_para->unseen_part->detail_map.push_back(detail_result[1][0].clone());
  //mesh_para->unseen_part->detail_map.push_back(detail_result[2][0].clone());



  std::shared_ptr<Shape> shape = mesh_para->unseen_part->cut_shape;
  STLVectori v_set = mesh_para->unseen_part->vertex_set;

  STLVectorf uv_list = model->getShapeUVCoord();
  STLVectorf color_list = model->getShapeColorList();
  const STLVectorf& cut_uv_list_hidden = shape->getUVCoord();
  for (size_t i = 0; i < cut_uv_list_hidden.size() / 2; ++i)
  {
    int winx = cut_uv_list_hidden[2 * i + 0] * resolution;
    int winy = cut_uv_list_hidden[2 * i + 1] * resolution;

    winy = winy < 0 ? 0 : (winy >= mesh_para->unseen_part->detail_map[0].rows ? mesh_para->unseen_part->detail_map[0].rows - 1 : winy);
    winx = winx < 0 ? 0 : (winx >= mesh_para->unseen_part->detail_map[0].cols ? mesh_para->unseen_part->detail_map[0].cols - 1 : winx);

    float new_color[3];
    new_color[0] = mesh_para->unseen_part->detail_map[0].at<float>(resolution - 1 - winy, winx);
    new_color[1] = mesh_para->unseen_part->detail_map[1].at<float>(resolution - 1 - winy, winx);
    new_color[2] = mesh_para->unseen_part->detail_map[2].at<float>(resolution - 1 - winy, winx);

    // get vertex id in original model
    color_list[3 * v_set[i] + 0] = new_color[0];
    color_list[3 * v_set[i] + 1] = new_color[1];
    color_list[3 * v_set[i] + 2] = new_color[2];

    // put uv for syn texture
    uv_list[2 * v_set[i] + 0] = cut_uv_list_hidden[2 * i + 0];
    uv_list[2 * v_set[i] + 1] = cut_uv_list_hidden[2 * i + 1];

    // put uv to hidden uv list
    hidden_uv_list[PolygonMesh::Vertex(v_set[i])] = Vec2(cut_uv_list_hidden[2 * i + 0], cut_uv_list_hidden[2 * i + 1]);

    // put uv tag for syn texture
    syn_texture_tag[PolygonMesh::Vertex(v_set[i])] = 1;
  }

  cv::Mat src_detail;
  std::vector<cv::Mat> seen_detail;
  seen_detail.push_back(mesh_para->seen_part->detail_map[2].clone());
  seen_detail.push_back(mesh_para->seen_part->detail_map[1].clone());
  seen_detail.push_back(mesh_para->seen_part->detail_map[0].clone());
  cv::merge(&seen_detail[0], 3, src_detail);
  //cv::minMaxLoc(src_detail,&min,&max);
  src_detail = src_detail / normalize_max;
  model->getOriRImg() = src_detail.clone();
  cv::imshow("source", (src_detail));

  shape = mesh_para->seen_part->cut_shape;
  v_set = mesh_para->seen_part->vertex_set;

  const STLVectorf& cut_uv_list = shape->getUVCoord();
  for (size_t i = 0; i < cut_uv_list.size() / 2; ++i)
  {
    int winx = cut_uv_list[2 * i + 0] * resolution;
    int winy = cut_uv_list[2 * i + 1] * resolution;

    winy = winy < 0 ? 0 : (winy >= mesh_para->seen_part->detail_map[0].rows ? mesh_para->seen_part->detail_map[0].rows - 1 : winy);
    winx = winx < 0 ? 0 : (winx >= mesh_para->seen_part->detail_map[0].cols ? mesh_para->seen_part->detail_map[0].cols - 1 : winx);

    float new_color[3];
    new_color[0] = mesh_para->seen_part->detail_map[0].at<float>(resolution - 1 - winy, winx);
    new_color[1] = mesh_para->seen_part->detail_map[1].at<float>(resolution - 1 - winy, winx);
    new_color[2] = mesh_para->seen_part->detail_map[2].at<float>(resolution - 1 - winy, winx);

    // get vertex id in original model
    color_list[3 * v_set[i] + 0] = new_color[0];
    color_list[3 * v_set[i] + 1] = new_color[1];
    color_list[3 * v_set[i] + 2] = new_color[2];

    // put uv for original texture
    uv_list[2 * v_set[i] + 0] = cut_uv_list[2 * i + 0];
    uv_list[2 * v_set[i] + 1] = cut_uv_list[2 * i + 1];

    // put uv tag for original texture
    //if (syn_texture_tag[PolygonMesh::Vertex(v_set[i])] != 1)
    //{
    syn_texture_tag[PolygonMesh::Vertex(v_set[i])] = 0;
    //}
    
  }

  model->updateColorList(color_list);
  model->updateUVCoord(uv_list);


  /*cv::FileStorage fs(model->getDataPath() + "/displacement_map.xml", cv::FileStorage::WRITE);
  fs << "displacement_map" << displacement_map;*/
  /*cv::FileStorage fs(model->getDataPath() + "/displacement_map.xml", cv::FileStorage::READ);
  cv::Mat disp_mat;
  fs["displacement_map"] >> disp_mat;*/

  /*computeDisplacementMap(model);
  computeFeatureMap(src_feature_map, TRUE);
  computeFeatureMap(tar_feature_map, FALSE);
  syn_tool.reset(new SynthesisTool);
  syn_tool->init(src_feature_map, tar_feature_map, displacement_map);
  syn_tool->doSynthesis();
  cv::Mat tar_detail = (syn_tool->getTargetDetail())[0];
  applyDisplacementMap(mesh_para->vertex_set_hidden, mesh_para->cut_shape_hidden, model, tar_detail);*/

  // test image synthesis
  //cv::Mat inputImage;
  //cv::imread("sample09-low.png").convertTo(inputImage, CV_32FC3);
  //inputImage = inputImage / 255.0;
  ///*cv::Mat grayImage = cv::Mat(inputImage.size().height,inputImage.size().width,CV_32FC1);
  //cv::cvtColor(inputImage,grayImage,CV_BGR2GRAY);
  //inputImage.convertTo(inputImage,CV_32FC1);*/
  //double min,max;
  //cv::minMaxLoc(inputImage,&min,&max);
  //cv::imshow("input", (inputImage - min) / (max - min));
  //syn_tool.reset(new SynthesisTool);
  //std::vector<cv::Mat> detail_image(3);
  //cv::split(inputImage, &detail_image[0]);
  //syn_tool->doImageSynthesis(detail_image);
  //cv::Mat tar_detail;
  //std::vector<cv::Mat> output_detail;
  //output_detail.push_back(syn_tool->getTargetDetail()[0][0].clone());
  //output_detail.push_back(syn_tool->getTargetDetail()[1][0].clone());
  //output_detail.push_back(syn_tool->getTargetDetail()[2][0].clone());
  //cv::merge(&output_detail[0], 3, tar_detail);
  //cv::minMaxLoc(tar_detail,&min,&max);
  //cv::imshow("result", (tar_detail - min) / (max - min));
  std::cout << "FINISHED!" << std::endl;
}

void DetailSynthesis::computeVectorField(std::shared_ptr<Model> model)
{
  /*curve_guided_vector_field.reset(new CurveGuidedVectorField);
  curve_guided_vector_field->computeVectorField(model);*/
  kevin_vector_field.reset(new KevinVectorField);
  kevin_vector_field->init(model);
  kevin_vector_field->compute_s_hvf();
  actors.clear();
  std::vector<GLActor> temp_actors;
  //curve_guided_vector_field->getDrawableActors(temp_actors);
  kevin_vector_field->getDrawableActors(temp_actors);
  for (size_t i = 0; i < temp_actors.size(); ++i)
  {
    actors.push_back(temp_actors[i]);
  }
}

void DetailSynthesis::getDrawableActors(std::vector<GLActor>& actors)
{
  actors = this->actors;
}

void DetailSynthesis::testShapePlane(std::shared_ptr<Model> model)
{
  int output_patch_id;
  std::vector<int> candidate;//(model->getPlaneCenter().size(), 1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  candidate.push_back(1);
  model->findCrspPatch(8, output_patch_id, candidate);

  std::vector<std::pair<Vector3f, Vector3f>> plane_center;
  plane_center.push_back(model->getPlaneCenter()[8]);
  plane_center.push_back(model->getPlaneCenter()[output_patch_id]);
  std::vector<std::pair<Vector3f, Vector3f>> original_plane_center;
  original_plane_center.push_back(model->getOriginalPlaneCenter()[8]);
  original_plane_center.push_back(model->getOriginalPlaneCenter()[output_patch_id]);
  //std::vector<std::set<int>> flat_surfaces = model->getFlatSurfaces();
  //VertexList vertex_list = model->getShapeVertexList();
  //FaceList face_list = model->getShapeFaceList();

  std::cout<<output_patch_id <<std::endl;

  actors.clear();
  actors.push_back(GLActor(ML_POINT, 5.0f));
  actors.push_back(GLActor(ML_LINE, 3.0f));

  for(auto j : plane_center)
  {
    actors[0].addElement(j.first(0), j.first(1), j.first(2), 1, 0, 0);
    actors[1].addElement(j.first(0), j.first(1), j.first(2), 0, 0, 0);
    actors[1].addElement(j.first(0) + 0.5 * j.second(0), j.first(1) + 0.5 * j.second(1), j.first(2) + 0.5 * j.second(2), 0, 0, 0);
    std::cout << j.first.transpose() << " " << j.second.transpose() << std::endl;
  }

  for(auto j : original_plane_center)
  {
    actors[0].addElement(j.first(0), j.first(1), j.first(2), 0, 1, 1);
    actors[1].addElement(j.first(0), j.first(1), j.first(2), 0.5, 1, 0);
    actors[1].addElement(j.first(0) + 0.5 * j.second(0), j.first(1) + 0.5 * j.second(1), j.first(2) + 0.5 * j.second(2), 0.5, 1, 0);
    std::cout << j.first.transpose() << " " << j.second.transpose() << std::endl;
  }


}

void DetailSynthesis::patchSynthesis(std::shared_ptr<Model> model)
{
  // do patch based synthesis

  // first iterate all patches to fill possible patches
  std::vector<int> candidate(mesh_para->shape_patches.size(), 0);
  for (size_t i = 0; i < mesh_para->shape_patches.size(); ++i)
  {
    if (mesh_para->shape_patches[i].filled == 0)
    {
      if (mesh_para->shape_patches[i].fill_ratio > 0.5)
      {
        syn_tool.reset(new SynthesisTool);
        syn_tool->doFilling(mesh_para->shape_patches[i].feature_map, mesh_para->shape_patches[i].detail_map);
        std::vector<std::vector<cv::Mat> >& detail_result = syn_tool->getTargetDetail();
        mesh_para->shape_patches[i].detail_map.clear();
        for (size_t i_ddim = 0; i_ddim < detail_result.size(); ++i_ddim)
        {
          mesh_para->shape_patches[i].detail_map.push_back(detail_result[i_ddim][0].clone());
        }
        mesh_para->shape_patches[i].filled = 1;
        mesh_para->shape_patches[i].fill_ratio = 1.0;
        candidate[i] = 1;
      }
    }
    else
    {
      candidate[i] = 1;
    }
  }

  // Then we synthesize left patches
  for (size_t i = 0; i < mesh_para->shape_patches.size(); ++i)
  {
    if (mesh_para->shape_patches[i].filled == 0)
    {
      int crsp_patch = 0;
      model->findCrspPatch(int(i), crsp_patch, candidate);
      
      syn_tool.reset(new SynthesisTool);
      syn_tool->init(mesh_para->shape_patches[crsp_patch].feature_map, mesh_para->shape_patches[i].feature_map, mesh_para->shape_patches[crsp_patch].detail_map);
      syn_tool->doSynthesisNew();
      
      std::vector<std::vector<cv::Mat> >& detail_result = syn_tool->getTargetDetail();
      mesh_para->shape_patches[i].detail_map.clear();
      for (size_t i_ddim = 0; i_ddim < detail_result.size(); ++i_ddim)
      {
        mesh_para->shape_patches[i].detail_map.push_back(detail_result[i_ddim][0].clone());
      }

      mesh_para->shape_patches[i].filled = 1;
      mesh_para->shape_patches[i].fill_ratio = 1.0;
      candidate[i] = 1;
    }
  }
}

void DetailSynthesis::mergeSynthesis(ParaShape* para_shape, std::shared_ptr<Model> model)
{
  // now every patch has details now
  // we merge them into seen and unseen
  int ddim = mesh_para->unseen_part->detail_map.size();

  std::shared_ptr<Shape> shape = mesh_para->unseen_part->cut_shape;
  std::shared_ptr<KDTreeWrapper> kdTree = mesh_para->unseen_part->kdTree_UV;
  STLVectori v_set = mesh_para->unseen_part->vertex_set;

  AdjList adjFaces_list = shape->getVertexShareFaces();
  std::vector<float> pt(2, 0);
  for(int x = 0; x < resolution; x ++)
  {
    for(int y = 0; y < resolution; y ++)
    {
      //int pt_id;
      //std::vector<float> pt;
      //pt.resize(2);
      //pt[0] = float(x) / resolution;
      //pt[1] = float(y) / resolution;
      //kdTree->nearestPt(pt,pt_id); // pt has been modified
      //std::vector<int> adjFaces = adjFaces_list[pt_id];
      //int face_id;
      //float point[3];
      //point[0] = float(x) / resolution;//pt[0];
      //point[1] = float(y) / resolution;;//pt[1];
      //point[2] = 0;
      //float lambda[3];
      //int id1,id2,id3;
      //for(size_t i = 0; i < adjFaces.size(); i ++)
      //{
      //  float l[3];
      //  int v1_id,v2_id,v3_id;
      //  float v1[3],v2[3],v3[3];
      //  v1_id = (shape->getFaceList())[3 * adjFaces[i]];
      //  v2_id = (shape->getFaceList())[3 * adjFaces[i] + 1];
      //  v3_id = (shape->getFaceList())[3 * adjFaces[i] + 2];
      //  v1[0] = (shape->getUVCoord())[2 * v1_id];
      //  v1[1] = (shape->getUVCoord())[2 * v1_id + 1];
      //  v1[2] = 0;
      //  v2[0] = (shape->getUVCoord())[2 * v2_id];
      //  v2[1] = (shape->getUVCoord())[2 * v2_id + 1];
      //  v2[2] = 0;
      //  v3[0] = (shape->getUVCoord())[2 * v3_id];
      //  v3[1] = (shape->getUVCoord())[2 * v3_id + 1];
      //  v3[2] = 0;
      //  ShapeUtility::computeBaryCentreCoord(point,v1,v2,v3,l);
      //  l[0] = (fabs(l[0]) < 1e-4) ? 0 : l[0];
      //  l[1] = (fabs(l[1]) < 1e-4) ? 0 : l[1];
      //  l[2] = (fabs(l[2]) < 1e-4) ? 0 : l[2];
      //  if(l[0] >= 0 && l[1] >= 0 && l[2] >= 0)
      //  {
      //    face_id = adjFaces[i];
      //    lambda[0] = l[0];
      //    lambda[1] = l[1];
      //    lambda[2] = l[2];
      //    id1 = v1_id;
      //    id2 = v2_id;
      //    id3 = v3_id;
      //  }
      //}
      pt[0] = float(x) / resolution;
      pt[1] = float(y) / resolution;
      int face_id;
      std::vector<int> id;
      std::vector<float> lambda;
      if(ShapeUtility::findClosestUVFace(pt, para_shape, lambda, face_id, id))
      {
        int patch_id = 0;
        int f_id_patch = 0;
        ShapeUtility::getFaceInPatchByFaceInMesh(mesh_para->unseen_part->face_set[face_id], mesh_para->shape_patches, f_id_patch, patch_id);

        // get vertex uv from that face in that patch
        const FaceList& f_list_patch = mesh_para->shape_patches[patch_id].cut_shape->getFaceList();
        const STLVectorf& uv_list_patch = mesh_para->shape_patches[patch_id].cut_shape->getUVCoord();
        Vector2f uv0(uv_list_patch[2 * f_list_patch[3 * f_id_patch + 0] + 0], uv_list_patch[2 * f_list_patch[3 * f_id_patch + 0] + 1]);
        Vector2f uv1(uv_list_patch[2 * f_list_patch[3 * f_id_patch + 1] + 0], uv_list_patch[2 * f_list_patch[3 * f_id_patch + 1] + 1]);
        Vector2f uv2(uv_list_patch[2 * f_list_patch[3 * f_id_patch + 2] + 0], uv_list_patch[2 * f_list_patch[3 * f_id_patch + 2] + 1]);
        Vector2f uv_bary = lambda[0] * uv0 + lambda[1] * uv1 + lambda[2] * uv2;

        int winx = uv_bary[0] * mesh_para->shape_patches[patch_id].detail_map[0].cols;
        int winy = uv_bary[1] * mesh_para->shape_patches[patch_id].detail_map[0].rows;

        winy = winy < 0 ? 0 : (winy >= mesh_para->shape_patches[patch_id].detail_map[0].rows ? mesh_para->shape_patches[patch_id].detail_map[0].rows - 1 : winy);
        winx = winx < 0 ? 0 : (winx >= mesh_para->shape_patches[patch_id].detail_map[0].cols ? mesh_para->shape_patches[patch_id].detail_map[0].cols - 1 : winx);

        // put the value in that detail map into this one
        for (int i = 0; i < ddim; ++i)
        {
          mesh_para->unseen_part->detail_map[i].at<float>(resolution - y - 1,x) = mesh_para->shape_patches[patch_id].detail_map[i].at<float>(resolution - winy - 1, winx);
        }
      }
      else
      {
        for (int i = 0; i < ddim; ++i)
        {
          mesh_para->unseen_part->detail_map[i].at<float>(resolution - y - 1,x) = -1.0;
        }
      }
    }
  }
}

void DetailSynthesis::doTransfer(std::shared_ptr<Model> src_model, std::shared_ptr<Model> tar_model)
{
  // 1. to do transfer, we first need to apply the displacement to src_model;

  this->testMeshPara(src_model);
  
  std::shared_ptr<ParaShape> src_para_shape(new ParaShape);
  src_para_shape->initWithExtShape(src_model);

  cv::FileStorage fs(src_model->getDataPath() + "/reflectance.xml", cv::FileStorage::READ);
  cv::Mat detail_reflectance_mat;
  fs["reflectance"] >> detail_reflectance_mat;

  cv::FileStorage fs2(src_model->getDataPath() + "/displacement.xml", cv::FileStorage::READ);
  cv::Mat displacement_mat;
  fs2["displacement"] >> displacement_mat;
  
  std::vector<cv::Mat> detail_image(3);
  cv::split(detail_reflectance_mat, &detail_image[0]);
  {
    // dilate the detail map in case of black
    for (int i = 0; i < 3; ++i)
    {
      ShapeUtility::dilateImage(detail_image[i], 15);
    }
  }
  std::vector<cv::Mat> new_detail_image;
  new_detail_image.push_back(detail_image[0]);
  new_detail_image.push_back(detail_image[1]);
  new_detail_image.push_back(detail_image[2]);
  new_detail_image.push_back(displacement_mat);

  computeDetailMap(src_para_shape.get(), new_detail_image, src_model, mesh_para->seen_part->cut_faces);

  NormalTransfer normal_transfer;
  std::string normal_file_name = "final_normal";
  //normal_transfer.prepareNewNormal(src_model, normal_file_name);
  //std::cout<<"test4"<< std::endl;
  //this->applyDisplacementMap(mesh_para->seen_part->vertex_set, mesh_para->seen_part->cut_shape, src_model, mesh_para->seen_part->detail_map[3]);

  {
    // try some real image
    //cv::Mat load_img = cv::imread(tar_model->getDataPath() + "/0.9-.png");
    //cv::Mat syn_ref_ext;
    //if (load_img.data != NULL)
    //{
    //  load_img.convertTo(syn_ref_ext, CV_32FC3);
    //  syn_ref_ext = syn_ref_ext / 255.0;
    //}
    //mesh_para->seen_part->detail_map.clear();
    //mesh_para->seen_part->detail_map.resize(3);
    //cv::split(syn_ref_ext, &mesh_para->seen_part->detail_map[0]);
    //this->resolution = 375;
  }

  //cv::FileStorage fs2(src_model->getDataPath() + "/displacement.xml", cv::FileStorage::READ);
  //cv::Mat displacement_mat;
  //fs2["displacement"] >> displacement_mat;
  //computeDisplacementMap(mesh_para->seen_part.get(), displacement_map, displacement_mat, src_model);

  // 2. second we need to compute the geometry feature on deformed src_model and tar_model;
  // normalized height, curvature, directional occlusion, surface normal
  ShapeUtility::computeNormalizedHeight(src_model);
  ShapeUtility::computeDirectionalOcclusion(src_model);
  ShapeUtility::computeSymmetry(src_model);
  ShapeUtility::computeSolidAngleCurvature(src_model);
  ShapeUtility::computeNormalizedHeight(tar_model);
  ShapeUtility::computeDirectionalOcclusion(tar_model);
  ShapeUtility::computeSymmetry(tar_model);
  ShapeUtility::computeSolidAngleCurvature(tar_model);

  // 3. third do CCA, skip for now

  // 4. prepare parameterized feature map and detail map of seen part (deformed) and feature map of tar_model
  // tar_model is parameterized by its original UV coordinate

  // prepare 
  PolygonMesh* poly_mesh = src_model->getPolygonMesh();
  PolygonMesh::Vertex_attribute<Scalar> normalized_height = poly_mesh->vertex_attribute<Scalar>("v:NormalizedHeight");
  PolygonMesh::Vertex_attribute<STLVectorf> directional_occlusion = poly_mesh->vertex_attribute<STLVectorf>("v:DirectionalOcclusion");
  PolygonMesh::Vertex_attribute<Vec3> v_normals = poly_mesh->vertex_attribute<Vec3>("v:normal");
  PolygonMesh::Vertex_attribute<std::vector<float>> v_symmetry = poly_mesh->vertex_attribute<std::vector<float>>("v:symmetry");
  PolygonMesh::Vertex_attribute<Vector4f> solid_angles = poly_mesh->vertex_attribute<Vector4f>("v:solid_angle");
  std::vector<std::vector<float> > vertex_feature_list(poly_mesh->n_vertices(), std::vector<float>());
  for (auto vit : poly_mesh->vertices())
  {
    vertex_feature_list[vit.idx()].push_back(normalized_height[vit]);
    vertex_feature_list[vit.idx()].push_back((v_normals[vit][0] + 1.0) / 2.0);
    vertex_feature_list[vit.idx()].push_back((v_normals[vit][1] + 1.0) / 2.0);
    vertex_feature_list[vit.idx()].push_back((v_normals[vit][2] + 1.0) / 2.0);
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](0));
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](1));
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](2));
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](3));
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][0]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][1]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][2]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][3]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][4]);
    for (size_t i = 0; i < directional_occlusion[vit].size(); ++i)
    {
      vertex_feature_list[vit.idx()].push_back(directional_occlusion[vit][i]/directional_occlusion[vit].size());
    }
  }
  //computeFeatureMap(mesh_para->seen_part.get(), vertex_feature_list);
  computeFeatureMap(src_para_shape.get(), vertex_feature_list);

  poly_mesh = tar_model->getPolygonMesh();
  normalized_height = poly_mesh->vertex_attribute<Scalar>("v:NormalizedHeight");
  directional_occlusion = poly_mesh->vertex_attribute<STLVectorf>("v:DirectionalOcclusion");
  v_normals = poly_mesh->vertex_attribute<Vec3>("v:normal");
  v_symmetry = poly_mesh->vertex_attribute<std::vector<float>>("v:symmetry");
  solid_angles = poly_mesh->vertex_attribute<Vector4f>("v:solid_angle");
  vertex_feature_list.clear();
  vertex_feature_list.resize(poly_mesh->n_vertices(), std::vector<float>());
  for (auto vit : poly_mesh->vertices())
  {
    vertex_feature_list[vit.idx()].push_back(normalized_height[vit]);
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](0));
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](1));
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](2));
    vertex_feature_list[vit.idx()].push_back(solid_angles[vit](3));
    vertex_feature_list[vit.idx()].push_back((v_normals[vit][0] + 1.0) / 2.0);
    vertex_feature_list[vit.idx()].push_back((v_normals[vit][1] + 1.0) / 2.0);
    vertex_feature_list[vit.idx()].push_back((v_normals[vit][2] + 1.0) / 2.0);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][0]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][1]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][2]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][3]);
    vertex_feature_list[vit.idx()].push_back(v_symmetry[vit][4]);
    for (size_t i = 0; i < directional_occlusion[vit].size(); ++i)
    {
      vertex_feature_list[vit.idx()].push_back(directional_occlusion[vit][i]/directional_occlusion[vit].size());
    }
  }
  std::shared_ptr<ParaShape> tar_para_shape(new ParaShape);
  tar_para_shape->initWithExtShape(tar_model);
  computeFeatureMap(tar_para_shape.get(), vertex_feature_list);

  //cv::FileStorage fs(src_model->getDataPath() + "/reflectance.xml", cv::FileStorage::READ);
  //cv::Mat detail_reflectance_mat;
  //fs["reflectance"] >> detail_reflectance_mat;

  //std::vector<cv::Mat> detail_image(3);
  //cv::split(detail_reflectance_mat, &detail_image[0]);
  //// dilate the detail map in case of black
  //for (int i = 0; i < 3; ++i)
  //{
  //  ShapeUtility::dilateImage(detail_image[i], 15);
  //}
  ////cv::merge(&detail_image[0], 3, detail_reflectance_mat);
  ////imshow("dilate souroce", detail_reflectance_mat);

  //computeDetailMap(mesh_para->seen_part.get(), detail_image, src_model, mesh_para->seen_part->cut_faces);
  //mesh_para->seen_part->detail_map.push_back(displacement_map.clone());

  // 5. do synthesis
  syn_tool.reset(new SynthesisTool);
  syn_tool->setExportPath(tar_model->getOutputPath());
  //syn_tool->init(mesh_para->seen_part->feature_map, tar_para_shape->feature_map, mesh_para->seen_part->detail_map);
  syn_tool->init(src_para_shape->feature_map, src_para_shape->feature_map, src_para_shape->detail_map);
  syn_tool->doSynthesisNew();

  std::vector<cv::Mat> result_detail;
  result_detail.push_back(syn_tool->getTargetDetail()[2][0].clone());
  result_detail.push_back(syn_tool->getTargetDetail()[1][0].clone());
  result_detail.push_back(syn_tool->getTargetDetail()[0][0].clone());
  cv::Mat result_reflectance;
  cv::merge(result_detail, result_reflectance);
  double min,max;
  cv::minMaxLoc(result_reflectance,&min,&max);
  if (normalize_max < 0) result_reflectance = result_reflectance / max;
  else result_reflectance = result_reflectance / normalize_max;
  cv::Mat result_displacement = syn_tool->getTargetDetail()[3][0].clone();
  applyDisplacementMap(tar_para_shape->vertex_set, tar_para_shape->cut_shape, tar_model, result_displacement);

  cv::imwrite(tar_model->getOutputPath() + "/reflectance.png", result_reflectance*255);
  cv::imwrite(tar_model->getOutputPath() + "/displacement.png", result_displacement*255);
  std::cout << "transfer finished." << std::endl;

  // 6. fill the detail map of tar_model
  this->startDetailSynthesis(src_model);

  cv::imwrite(src_model->getOutputPath() + "/displacement.png", 255*mesh_para->seen_part->detail_map[3]);
}