
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "bee/Converter.Test.h"
#include <doctest/doctest.h>
#include <fbxsdk.h>
#include <fx/gltf.h>
#include <string>

void createFbxGrid(const char *path_, std::uint32_t N) {
  // Initialize the FBX SDK
  const auto fbxManager = fbxsdk::FbxManager::Create();
  auto ios = FbxIOSettings::Create(fbxManager, IOSROOT);
  fbxManager->SetIOSettings(ios);
  // Create an FBX scene
  const auto fbxScene = fbxsdk::FbxScene::Create(fbxManager, "Test");
  // Create a grid mesh
  const auto gridNode = fbxsdk::FbxNode::Create(fbxManager, "Gird");
  const auto grid = fbxsdk::FbxMesh::Create(fbxManager, "Grid");

    FbxVector4 *vertices = new FbxVector4[N * N];
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
          vertices[i * N + j] = FbxVector4((float)i, 0.0, (float)j);
        }
    }
    // Set the vertices of the grid
    grid->InitControlPoints(N * N);
    for (int i = 0; i < N * N; i++) {
    grid->SetControlPointAt(vertices[i], i);
    }
    // Define the faces of the grid
    int *indices = new int[(N - 1) * (N - 1) * 4];
    int index = 0;
    for (int i = 0; i < N - 1; i++) {
        for (int j = 0; j < N - 1; j++) {
          indices[index++] = i * N + j;
          indices[index++] = i * N + j + 1;
          indices[index++] = (i + 1) * N + j + 1;
          indices[index++] = (i + 1) * N + j;
        }
    }
    for (int i = 0; i < (N - 1) * (N - 1); i++) {
        grid->BeginPolygon(-1, -1, false);
        grid->AddPolygon(indices[i * 4 + 0]);
        grid->AddPolygon(indices[i * 4 + 1]);
        grid->AddPolygon(indices[i * 4 + 2]);
        grid->AddPolygon(indices[i * 4 + 3]);
        grid->EndPolygon();
    }
    gridNode->SetNodeAttribute(grid);
    // Add the grid node to the scene
    fbxScene->GetRootNode()->AddChild(gridNode);
    // Create and assign a material to the mesh
    FbxSurfacePhong *pMaterial =
        FbxSurfacePhong::Create(fbxManager, "Material");
    gridNode->AddMaterial(pMaterial);
    const auto fbxFileName = path_;
    const auto exporter = fbxsdk::FbxExporter::Create(fbxManager, "");
    const auto success =
        exporter->Initialize(fbxFileName, -1, fbxManager->GetIOSettings());
    CHECK_EQ(success, true);
    const auto exportSuccess = exporter->Export(fbxScene);
    //std::printf("Error returned: %s\n\n", exporter->GetStatus().GetErrorString());
    CHECK_EQ(exportSuccess, true);

    exporter->Destroy();
    //fbxScene->Destroy(true);
    fbxManager->Destroy();
}
FbxNode* createGrid(FbxManager *pSdkManager, FbxScene *pScene, int rows, int cols) {

    // Create a new mesh
    FbxMesh *pMesh = FbxMesh::Create(pSdkManager, "Grid");

    // Initialize the control points
    int pointCount = rows * cols;
    pMesh->InitControlPoints(pointCount);
    FbxVector4 *controlPoints = pMesh->GetControlPoints();

    // Create the vertices
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
          controlPoints[i * cols + j].Set(j, 0, i);
        }
    }

    // Create the polygons
    for (int i = 0; i < rows - 1; i++) {
        for (int j = 0; j < cols - 1; j++) {
          pMesh->BeginPolygon();
          pMesh->AddPolygon(i * cols + j);
          pMesh->AddPolygon((i + 1) * cols + j);
          pMesh->AddPolygon((i + 1) * cols + j + 1);
          pMesh->EndPolygon();

          pMesh->BeginPolygon();
          pMesh->AddPolygon(i * cols + j);
          pMesh->AddPolygon((i + 1) * cols + j + 1);
          pMesh->AddPolygon(i * cols + j + 1);
          pMesh->EndPolygon();
        }
    }

    // Create a node to contain the mesh
    FbxNode *pNode = FbxNode::Create(pScene, "GridNode");
    pNode->SetNodeAttribute(pMesh);
    pScene->GetRootNode()->AddChild(pNode);

    // Create and assign a material to the mesh
    FbxSurfacePhong *pMaterial =
        FbxSurfacePhong::Create(pSdkManager, "Material");
    pNode->AddMaterial(pMaterial);

    return pNode;
}
void testIndexUnit(const char *path_,
                   std::uint32_t vertex_count_,
                   fx::gltf::Accessor::ComponentType component_type_) {
  const auto fbxFileName = path_;
  createFbxGrid(fbxFileName, (uint32_t)std::sqrt(vertex_count_));
  bee::ConvertOptions convertOptions;
  auto glTF = bee::_convert_test(reinterpret_cast<const char8_t *>(fbxFileName),
                                 convertOptions);

  const auto &glTFDocument = glTF.document();
  CHECK_EQ(glTFDocument.meshes.size(), 1);
  CHECK_EQ(glTFDocument.meshes[0].primitives.size(), 1);
  const auto &indexAccessor =
      glTFDocument.accessors[glTFDocument.meshes[0].primitives[0].indices];
  CHECK_EQ(indexAccessor.componentType, component_type_);
}

TEST_CASE("Mesh") {
  SUBCASE("Index unit") {
    // 生成65535以下数量的顶点的FBX
    // 调用 API 转换乘 gltf
    // 再去判断
    testIndexUnit("Grid_256A.fbx", 256 * 256  ,
                   fx::gltf::Accessor::ComponentType::UnsignedInt);
    testIndexUnit("Grid_255A.fbx", 255 * 255  ,
                  fx::gltf::Accessor::ComponentType::UnsignedShort);
  }
}