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
    Guard(fbxsdk::FbxManager &manager_)
        : _manager(&manager_) {
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

const auto get_gltf_node_by_name = [](const fx::gltf::Document &gltf_document_,
                                      std::string_view name_) -> const fx::gltf::Node & {
  const auto rNode = std::ranges::find_if(
      gltf_document_.nodes,
      [name_](const auto &node_) { return node_.name == name_; });
  CHECK_NE(rNode, gltf_document_.nodes.end());
  return *rNode;
};

const auto count_gltf_mesh_references = [](const fx::gltf::Document &gltf_document_,
                                           int mesh_index_) {
  return std::ranges::count_if(gltf_document_.nodes,
                               [mesh_index_](const auto &node_) {
                                 return node_.mesh == mesh_index_;
                               });
};

TEST_CASE("Mesh") {
  SUBCASE("Index unit") {
    // test 65538 index
    testIndexUnit("T_65538.fbx", 65538,
                  fx::gltf::Accessor::ComponentType::UnsignedInt);
    // test 65535 index
    testIndexUnit("T_65535.fbx", 65535,
                  fx::gltf::Accessor::ComponentType::UnsignedShort);
  }

  SUBCASE("Mesh naming") {
    struct Case {
      std::vector<std::string_view> inputs;
      std::string_view expectation;
    };

    const std::vector<Case> cases = {
        {
            {"a"}, // inputs
            "a",   // expectation
        },
        {
            {"a", "a"},
            "a",
        },
        {
            {"a", "b"},
            "a, b",
        },
        {
            {"b", "a"},
            "a, b",
        },
        {
            {""},
            "",
        },
        {
            {"", ""},
            "",
        },
        {
            {"", "", ""},
            "",
        },
        {
            {"a", ""},
            "a",
        },
        {
            {"b", "", "a", "", "b", "c", "a"}, // inputs
            "a, b, c",                         // expectation
        },
        {
            {"a", "\U0001F60A", "\U0001F60A"},
            "\U0001F60A, a",
        },
    };

    const auto fixture = create_fbx_scene_fixture(
        [&cases](fbxsdk::FbxManager &manager_) -> fbxsdk::FbxScene & {
          const auto scene = fbxsdk::FbxScene::Create(&manager_, "myScene");

          for (const auto iCase : std::views::iota(0u, cases.size())) {
            const auto &theCase = cases[iCase];
            const auto nodeName = fmt::format("node-{}", iCase);
            const auto node = fbxsdk::FbxNode::Create(scene, nodeName.c_str());
            CHECK_UNARY(scene->GetRootNode()->AddChild(node));
            for (const auto meshName : theCase.inputs) {
              const auto mesh = fbxsdk::FbxMesh::Create(scene, meshName.data());
              CHECK_UNARY(node->AddNodeAttribute(mesh));
            }
          }

          return *scene;
        });

    bee::ConvertOptions options;
    const auto result = bee::_convert_test(fixture.path().u8string(), options);

    CHECK_EQ(result.document().nodes.size(), cases.size());
    for (const auto iCase : std::views::iota(0u, cases.size())) {
      const auto &theCase = cases[iCase];
      const auto nodeName = fmt::format("node-{}", iCase);
      const auto &node = get_gltf_node_by_name(result.document(), nodeName);
      CHECK_GE(node.mesh, 0);
      const auto &mesh = result.document().meshes[node.mesh];
      CHECK_EQ(mesh.name, theCase.expectation);
    }
  }

  SUBCASE("Mesh instancing") {
    SUBCASE("General") {
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
                    scene, "node-ref-to-shared-mesh-but-have-more-than-one-mesh");
                CHECK_UNARY(scene->GetRootNode()->AddChild(node));
                CHECK_UNARY(node->AddNodeAttribute(mesh));
                CHECK_UNARY(node->AddNodeAttribute(
                    fbxsdk::FbxMesh::Create(scene, "some-mesh-2")));
              }

              {
                const auto sharedMesh2 =
                    fbxsdk::FbxMesh::Create(scene, "some-shared-mesh-2");

                {
                  const auto node = fbxsdk::FbxNode::Create(
                      scene, "node-ref-to-shared-mesh1-then-shared-mesh2");
                  CHECK_UNARY(scene->GetRootNode()->AddChild(node));
                  CHECK_UNARY(node->AddNodeAttribute(mesh));
                  CHECK_UNARY(node->AddNodeAttribute(sharedMesh2));
                }
                {
                  const auto node = fbxsdk::FbxNode::Create(
                      scene, "another-node-ref-to-shared-mesh1-then-shared-mesh2");
                  CHECK_UNARY(scene->GetRootNode()->AddChild(node));
                  CHECK_UNARY(node->AddNodeAttribute(mesh));
                  CHECK_UNARY(node->AddNodeAttribute(sharedMesh2));
                }
                {
                  const auto node = fbxsdk::FbxNode::Create(
                      scene, "another-node-ref-to-shared-mesh2-then-shared-mesh1");
                  CHECK_UNARY(scene->GetRootNode()->AddChild(node));
                  CHECK_UNARY(node->AddNodeAttribute(sharedMesh2));
                  CHECK_UNARY(node->AddNodeAttribute(mesh));
                }
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

      {
        const auto &node1 =
            get_gltf_node_by_name(result.document(), "node-ref-to-shared-mesh");
        const auto &node2 =
            get_gltf_node_by_name(result.document(), "node2-ref-to-shared-mesh");
        CHECK_EQ(node1.mesh, node2.mesh);
        CHECK_EQ(count_gltf_mesh_references(result.document(), node1.mesh), 2);
        CHECK_EQ(result.document().meshes[node1.mesh].name, "some-shared-mesh");
      }

      {
        const auto &node1 = get_gltf_node_by_name(result.document(),
                                                  "node-ref-to-anonymous-mesh-instance");
        const auto &node2 = get_gltf_node_by_name(result.document(),
                                                  "node2-ref-to-anonymous-mesh-instance");
        CHECK_EQ(node1.mesh, node2.mesh);
        CHECK_EQ(count_gltf_mesh_references(result.document(), node1.mesh), 2);
        CHECK_EQ(result.document().meshes[node1.mesh].name, "");
      }

      {

        const auto &node1 = get_gltf_node_by_name(result.document(),
                                                  "node-ref-to-shared-mesh1-then-shared-mesh2");
        const auto &node2 = get_gltf_node_by_name(result.document(),
                                                  "another-node-ref-to-shared-mesh1-then-shared-mesh2");
        const auto &node3 = get_gltf_node_by_name(result.document(),
                                                  "another-node-ref-to-shared-mesh2-then-shared-mesh1");
        CHECK_GE(node1.mesh, 0);
        CHECK_EQ(node1.mesh, node2.mesh);
        CHECK_EQ(node1.mesh, node3.mesh);
        CHECK_EQ(count_gltf_mesh_references(result.document(), node1.mesh), 3);
        CHECK_EQ(result.document().meshes[node1.mesh].name, "some-shared-mesh, some-shared-mesh-2");
      }
    }

    SUBCASE("Does not form a instancing case") {
      const auto fixture = create_fbx_scene_fixture(
          [](fbxsdk::FbxManager &manager_) -> fbxsdk::FbxScene & {
            const auto scene = fbxsdk::FbxScene::Create(&manager_, "myScene");

            const auto sharingMesh = fbxsdk::FbxMesh::Create(scene, "some-shared-mesh");
            const auto exclusiveMesh = fbxsdk::FbxMesh::Create(scene, "some-exclusive-mesh");

            {
              const auto node =
                  fbxsdk::FbxNode::Create(scene, "node-ref-to-shared-mesh");
              CHECK_UNARY(scene->GetRootNode()->AddChild(node));
              CHECK_UNARY(node->AddNodeAttribute(sharingMesh));
            }

            {
              const auto node =
                  fbxsdk::FbxNode::Create(scene, "node-ref-to-shared-mesh-then-exclusive-mesh");
              CHECK_UNARY(scene->GetRootNode()->AddChild(node));
              CHECK_UNARY(node->AddNodeAttribute(sharingMesh));
              CHECK_UNARY(node->AddNodeAttribute(exclusiveMesh));
            }

            for (const auto i : std::views::iota(0, 2)) {
              const auto node =
                  fbxsdk::FbxNode::Create(scene, fmt::format("node{}-ref-to-shared-mesh-with-diff-material", i).c_str());
              CHECK_UNARY(scene->GetRootNode()->AddChild(node));
              CHECK_UNARY(node->AddNodeAttribute(sharingMesh));
              const auto material = fbxsdk::FbxSurfacePhong::Create(&manager_, fmt::format("some-material-{}", i).c_str());
              CHECK_GE(node->AddMaterial(material), 0);
            }

            {
              const auto node = fbxsdk::FbxNode::Create(
                  scene,
                  "node-ref-to-shared-mesh-but-have-geometrix-transform");
              CHECK_UNARY(scene->GetRootNode()->AddChild(node));
              CHECK_UNARY(node->AddNodeAttribute(sharingMesh));
              node->SetGeometricTranslation(
                  fbxsdk::FbxNode::EPivotSet::eSourcePivot,
                  fbxsdk::FbxVector4{1., 2., 3.});
            }

            return *scene;
          });

      bee::ConvertOptions options;
      const auto result = bee::_convert_test(fixture.path().u8string(), options);

      CHECK_EQ(result.document().nodes.size(), 5);
      CHECK_EQ(result.document().meshes.size(), 5);

      {
        const auto &node =
            get_gltf_node_by_name(result.document(), "node-ref-to-shared-mesh");
        CHECK_EQ(count_gltf_mesh_references(result.document(), node.mesh), 1);
        const auto &mesh = result.document().meshes[node.mesh];
        CHECK_EQ(mesh.name, "some-shared-mesh");
      }

      {
        const auto &node =
            get_gltf_node_by_name(result.document(), "node-ref-to-shared-mesh-then-exclusive-mesh");
        CHECK_EQ(count_gltf_mesh_references(result.document(), node.mesh), 1);
        const auto &mesh = result.document().meshes[node.mesh];
        CHECK_EQ(mesh.name, "some-exclusive-mesh, some-shared-mesh");
      }

      for (const auto i : std::views::iota(0, 2)) {
        const auto &node =
            get_gltf_node_by_name(result.document(), fmt::format("node{}-ref-to-shared-mesh-with-diff-material", i));
        CHECK_GE(node.mesh, 0);
        CHECK_EQ(count_gltf_mesh_references(result.document(), node.mesh), 1);
        const auto &mesh = result.document().meshes[node.mesh];
        CHECK_EQ(mesh.name, "some-shared-mesh");
      }

      {
        const auto &node =
            get_gltf_node_by_name(result.document(),
                                  "node-ref-to-shared-mesh-but-have-geometrix-transform");
        CHECK_EQ(count_gltf_mesh_references(result.document(), node.mesh), 1);
        CHECK_EQ(result.document().meshes[node.mesh].name, "some-shared-mesh");
      }
    }
  }

  SUBCASE("Splitting") {
    SUBCASE("Split an empty mesh") {
      const auto fixture = create_fbx_scene_fixture(
          [](fbxsdk::FbxManager &manager_) -> fbxsdk::FbxScene & {
            const auto scene = fbxsdk::FbxScene::Create(&manager_, "myScene");

            const auto mesh =
                fbxsdk::FbxMesh::Create(scene, "some-mesh");

            const auto node =
                fbxsdk::FbxNode::Create(scene, "some-node");
            CHECK_UNARY(scene->GetRootNode()->AddChild(node));
            CHECK_UNARY(node->AddNodeAttribute(mesh));

            return *scene;
          });

      bee::ConvertOptions options;
      auto result = bee::_convert_test(fixture.path().u8string(), options);
    }
  }

  SUBCASE("Split an sharing mesh") {
    const auto fixture = create_fbx_scene_fixture(
        [](fbxsdk::FbxManager &manager_) -> fbxsdk::FbxScene & {
          const auto scene = fbxsdk::FbxScene::Create(&manager_, "myScene");

          auto materials = std::vector<fbxsdk::FbxSurfaceMaterial *>{};
          materials.reserve(2);
          std::ranges::copy(
              std::views::iota(0, 2) |
                  std::views::transform([&manager_](auto index_) -> fbxsdk::FbxSurfaceMaterial * {
                    return fbxsdk::FbxSurfacePhong::Create(&manager_, fmt::format("some-material-{}", index_).c_str());
                  }),
              std::back_inserter(materials));

          const auto mesh =
              fbxsdk::FbxMesh::Create(scene, "some-multiple-materials-sharing-mesh");
          mesh->InitControlPoints(4);
          const auto mat = mesh->CreateElementMaterial();
          mat->SetReferenceMode(fbxsdk::FbxLayerElement::EReferenceMode::eIndexToDirect);
          mat->SetMappingMode(fbxsdk::FbxLayerElement::EMappingMode::eByPolygon);
          mesh->BeginPolygon(0);
          mesh->AddPolygon(0);
          mesh->AddPolygon(1);
          mesh->AddPolygon(2);
          mesh->EndPolygon();
          mesh->BeginPolygon(1);
          mesh->AddPolygon(1);
          mesh->AddPolygon(2);
          mesh->AddPolygon(3);
          mesh->EndPolygon();

          for (const auto i : std::views::iota(0, 2)) {
            const auto node =
                fbxsdk::FbxNode::Create(scene, fmt::format("node{}-ref-to-shared-mesh", i).c_str());
            CHECK_UNARY(scene->GetRootNode()->AddChild(node));
            CHECK_UNARY(node->AddNodeAttribute(mesh));
            CHECK_GE(node->AddMaterial(materials[0]), 0);
            CHECK_GE(node->AddMaterial(materials[1]), 0);
          }

          return *scene;
        });

    bee::ConvertOptions options;
    auto result = bee::_convert_test(fixture.path().u8string(), options);

    CHECK_EQ(result.document().nodes.size(), 2);
    CHECK_EQ(result.document().materials.size(), 2);

    CHECK_EQ(result.document().meshes.size(), 1);
    {
      const auto &mesh = result.document().meshes.front();
      CHECK_EQ(mesh.name, "some-multiple-materials-sharing-mesh");
      CHECK_EQ(mesh.primitives.size(), 2);
    }

    for (const auto i : std::views::iota(0, 2)) {
      const auto &node =
          get_gltf_node_by_name(result.document(), fmt::format("node{}-ref-to-shared-mesh", i));
    }
  }
}
