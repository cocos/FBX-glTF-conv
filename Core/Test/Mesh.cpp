#include <bee/Converter.Test.h>
#include <doctest/doctest.h>
#include <fbxsdk.h>
#include <filesystem>
#include <fmt/format.h>
#include <fx/gltf.h>
#include <ranges>
#include <string>
#include <string_view>

auto create_fbx_scene_fixture(
    std::function<fbxsdk::FbxScene &(fbxsdk::FbxManager &manager_)> callback_) {

  const auto manager = fbxsdk::FbxManager::Create();

  struct Guard {
    Guard(fbxsdk::FbxManager &manager_) : _manager(&manager_) {
    }

    ~Guard() {
      if (_manager) {
        _manager->Destroy();
        _manager = nullptr;
      }
    }

  private:
    fbxsdk::FbxManager *_manager;
  };

  Guard guard{*manager};

  auto &fbxScene = callback_(*manager);

  static int counter = 0;
  const auto fbxPath = std::filesystem::temp_directory_path() /
                       u8"FBX-glTF-conv-test" /
                       fmt::format("{}.fbx", counter++);
  std::filesystem::create_directories(fbxPath.parent_path());

  const auto exporter =
      fbxsdk::FbxExporter::Create(fbxScene.GetFbxManager(), "");
  const auto exportStatus = exporter->Initialize(fbxPath.string().c_str(), -1);
  CHECK_UNARY(exportStatus);
  exporter->Export(&fbxScene);

  exporter->Destroy();

  struct Fixture {
    Fixture(std::filesystem::path path_) noexcept
        : _path(path_), _deleted(false) {
    }

    Fixture(Fixture &&other_) noexcept {
      std::swap(this->_deleted, other_._deleted);
      std::swap(this->_path, other_._path);
    }

    ~Fixture() {
      if (!_deleted) {
        _deleted = true;
        std::filesystem::remove(_path);
      }
    }

    const auto &path() const {
      return this->_path;
    }

  private:
    std::filesystem::path _path;
    bool _deleted = true;
  };

  return Fixture{fbxPath};
}

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

  std::vector<FbxVector4> vertices;
  vertices.reserve(N * N);
  for (decltype(N) i = 0; i < N; i++) {
    for (decltype(N) j = 0; j < N; j++) {
      vertices.push_back(FbxVector4((float)i, 0.0, (float)j));
    }
  }
  // Set the vertices of the grid
  grid->InitControlPoints(N * N);
  for (decltype(N) i = 0; i < N * N; i++) {
    grid->SetControlPointAt(vertices[i], i);
  }
  // Define the faces of the grid
  std::vector<int> indices;
  indices.reserve((N - 1) * (N - 1) * 4);
  for (decltype(N) i = 0; i < N - 1; i++) {
    for (decltype(N) j = 0; j < N - 1; j++) {
      indices.push_back(i * N + j);
      indices.push_back(i * N + j + 1);
      indices.push_back((i + 1) * N + j + 1);
      indices.push_back((i + 1) * N + j);
    }
  }
  for (decltype(N) i = 0; i < (N - 1) * (N - 1); i++) {
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
  FbxSurfacePhong *pMaterial = FbxSurfacePhong::Create(fbxManager, "Material");
  gridNode->AddMaterial(pMaterial);
  const auto fbxFileName = path_;
  const auto exporter = fbxsdk::FbxExporter::Create(fbxManager, "");
  const auto success =
      exporter->Initialize(fbxFileName, -1, fbxManager->GetIOSettings());
  CHECK_EQ(success, true);
  const auto exportSuccess = exporter->Export(fbxScene);
  CHECK_EQ(exportSuccess, true);
  exporter->Destroy();
  fbxManager->Destroy();
}

// N : total triangles
void createTriangles(const char *path_, int N, int M = 200) {
  // Initialize the FBX SDK
  FbxManager *manager = FbxManager::Create();

  // Create an FBX scene
  FbxScene *scene = FbxScene::Create(manager, "MyScene");

  // Create a Node
  FbxNode *trianglesNode = FbxNode::Create(scene, "MyTriangles");

  // Create a mesh for the triangles
  FbxMesh *triangleMesh = FbxMesh::Create(scene, "MyTrianglesMesh");
  trianglesNode->AddNodeAttribute(triangleMesh);

  // Create control points for the triangle mesh
  int controlPointsCount = 3 * N;
  triangleMesh->InitControlPoints(controlPointsCount);

  // Set the control point positions
  FbxVector4 *controlPoints = triangleMesh->GetControlPoints();
  int count = 0;
  for (decltype(N) i = 0; i < N; i++) {
    double x = i % M * 1.1;
    double y = i / M * 1.1;
    double z = 0;
    controlPoints[count++] = FbxVector4(x, y, z);
    controlPoints[count++] = FbxVector4(x + 1, y, z);
    controlPoints[count++] = FbxVector4(x + 1, y + 1, z);
  }

  // Create triangles
  int triangleCount = N;
  for (decltype(N) i = 0; i < triangleCount; i++) {
    triangleMesh->BeginPolygon();
    triangleMesh->AddPolygon(i * 3);
    triangleMesh->AddPolygon(i * 3 + 1);
    triangleMesh->AddPolygon(i * 3 + 2);
    triangleMesh->EndPolygon();
  }
  scene->GetRootNode()->AddChild(trianglesNode);
  FbxExporter *exporter = FbxExporter::Create(manager, "MyExporter");
  exporter->Initialize(path_, -1, manager->GetIOSettings());
  exporter->Export(scene);
  exporter->Destroy();
  manager->Destroy();
}
void testIndexUnit(const char *path_,
                   std::uint32_t vertex_count_,
                   fx::gltf::Accessor::ComponentType component_type_) {
  const auto fbxFileName = path_;
  createTriangles(fbxFileName, vertex_count_ / 3);
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
    // test 65538 index
    testIndexUnit("T_65538.fbx", 65538,
                  fx::gltf::Accessor::ComponentType::UnsignedInt);
    // test 65535 index
    testIndexUnit("T_65535.fbx", 65535,
                  fx::gltf::Accessor::ComponentType::UnsignedShort);
  }

  SUBCASE("Mesh instancing") {
    const auto fixture = create_fbx_scene_fixture(
        [](fbxsdk::FbxManager &manager_) -> fbxsdk::FbxScene & {
          const auto scene = fbxsdk::FbxScene::Create(&manager_, "myScene");

          {
            const auto mesh =
                fbxsdk::FbxMesh::Create(scene, "some-shared-mesh");

            {
              const auto node1 =
                  fbxsdk::FbxNode::Create(scene, "node-ref-to-shared-mesh");
              CHECK_UNARY(scene->GetRootNode()->AddChild(node1));
              CHECK_UNARY(node1->AddNodeAttribute(mesh));

              const auto node2 =
                  fbxsdk::FbxNode::Create(scene, "node2-ref-to-shared-mesh");
              CHECK_UNARY(scene->GetRootNode()->AddChild(node2));
              CHECK_UNARY(node2->AddNodeAttribute(mesh));
            }

            {
              const auto node = fbxsdk::FbxNode::Create(
                  scene,
                  "node-ref-to-shared-mesh-but-have-geometrix-transform");
              CHECK_UNARY(scene->GetRootNode()->AddChild(node));
              CHECK_UNARY(node->AddNodeAttribute(mesh));
              node->SetGeometricTranslation(
                  fbxsdk::FbxNode::EPivotSet::eSourcePivot,
                  fbxsdk::FbxVector4{1., 2., 3.});
            }

            {
              const auto node = fbxsdk::FbxNode::Create(
                  scene, "node-ref-to-shared-mesh-but-have-more-than-one-mesh");
              CHECK_UNARY(scene->GetRootNode()->AddChild(node));
              CHECK_UNARY(node->AddNodeAttribute(mesh));
              CHECK_UNARY(node->AddNodeAttribute(
                  fbxsdk::FbxMesh::Create(scene, "some-mesh-2")));
            }
          }

          {
            auto node1 = fbxsdk::FbxNode::Create(
                scene, "node-ref-to-anonymous-mesh-instance");
            CHECK_UNARY(scene->GetRootNode()->AddChild(node1));
            auto node2 = fbxsdk::FbxNode::Create(
                scene, "node2-ref-to-anonymous-mesh-instance");
            CHECK_UNARY(scene->GetRootNode()->AddChild(node2));
            auto mesh = fbxsdk::FbxMesh::Create(scene, "");
            CHECK_UNARY(node1->AddNodeAttribute(mesh));
            CHECK_UNARY(node2->AddNodeAttribute(mesh));
          }

          return *scene;
        });

    bee::ConvertOptions options;
    auto result = bee::_convert_test(fixture.path().u8string(), options);

    CHECK_EQ(result.document().meshes.size(), 4);

    const auto getNodeByName = [](fx::gltf::Document &gltf_document_,
                                  std::string_view name_) -> fx::gltf::Node & {
      const auto rNode = std::ranges::find_if(
          gltf_document_.nodes,
          [name_](const auto &node_) { return node_.name == name_; });
      CHECK_NE(rNode, gltf_document_.nodes.end());
      return *rNode;
    };

    const auto countMeshReferences = [](fx::gltf::Document &gltf_document_,
                                        int mesh_index_) {
      return std::ranges::count_if(gltf_document_.nodes,
                                   [mesh_index_](const auto &node_) {
                                     return node_.mesh == mesh_index_;
                                   });
    };

    {
      const auto &node1 =
          getNodeByName(result.document(), "node-ref-to-shared-mesh");
      const auto &node2 =
          getNodeByName(result.document(), "node2-ref-to-shared-mesh");
      CHECK_EQ(node1.mesh, node2.mesh);
      CHECK_EQ(countMeshReferences(result.document(), node1.mesh), 2);
      CHECK_EQ(result.document().meshes[node1.mesh].name, "some-shared-mesh");
    }

    {
      const auto &node =
          getNodeByName(result.document(),
                        "node-ref-to-shared-mesh-but-have-geometrix-transform");
      CHECK_EQ(countMeshReferences(result.document(), node.mesh), 1);
      CHECK_EQ(result.document().meshes[node.mesh].name, "some-shared-mesh");
    }

    {
      const auto &node =
          getNodeByName(result.document(),
                        "node-ref-to-shared-mesh-but-have-more-than-one-mesh");
      CHECK_EQ(countMeshReferences(result.document(), node.mesh), 1);
      CHECK_EQ(result.document().meshes[node.mesh].name, "some-shared-mesh");
    }

    {
      const auto &node1 = getNodeByName(result.document(),
                                        "node-ref-to-anonymous-mesh-instance");
      const auto &node2 = getNodeByName(result.document(),
                                        "node2-ref-to-anonymous-mesh-instance");
      CHECK_EQ(node1.mesh, node2.mesh);
      CHECK_EQ(countMeshReferences(result.document(), node1.mesh), 2);
      CHECK_EQ(result.document().meshes[node1.mesh].name, "");
    }
  }
}
