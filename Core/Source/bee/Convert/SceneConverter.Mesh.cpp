
#include <bee/Convert/ConvertError.h>
#include <bee/Convert/SceneConverter.h>
#include <bee/Convert/fbxsdk/Spreader.h>
#include <bee/UntypedVertex.h>
#include <fmt/format.h>

namespace bee {
/// <summary>
/// We're unable to process multi material layers.
/// </summary>
class MultiMaterialLayersError : public MeshError {
public:
  using MeshError::MeshError;
};

void to_json(nlohmann::json &j_, const MultiMaterialLayersError &error_) {
  j_ = nlohmann::json{{"node", error_.node()}};
}

template <typename Dst_, typename Src_, std::size_t N_>
static void untypedVertexCopy(std::byte *out_, const std::byte *in_) {
  auto in = reinterpret_cast<const Src_ *>(in_);
  auto out = reinterpret_cast<Dst_ *>(out_);
  for (int i = 0; i < N_; ++i) {
    out[i] = static_cast<Dst_>(in[i]);
  }
}

template <typename Dst_, typename Src_>
static std::function<void(std::byte *out_, const std::byte *in_)>
makeUntypedVertexCopyN(std::size_t n_) {
  return [n_](std::byte *out_, const std::byte *in_) {
    auto in = reinterpret_cast<const Src_ *>(in_);
    auto out = reinterpret_cast<Dst_ *>(out_);
    for (int i = 0; i < n_; ++i) {
      out[i] = static_cast<Dst_>(in[i]);
    }
  };
}

std::optional<SceneConverter::ConvertMeshResult>
SceneConverter::_convertNodeMeshes(
    FbxNodeDumpMeta &node_meta_,
    const std::vector<fbxsdk::FbxMesh *> &fbx_meshes_,
    fbxsdk::FbxNode &fbx_node_) {
  assert(!fbx_meshes_.empty());

  auto meshName = _getName(*fbx_meshes_.front(), fbx_node_);
  auto [vertexTransform, normalTransform] = _getGeometrixTransform(fbx_node_);
  auto vertexTransformX =
      (vertexTransform == fbxsdk::FbxMatrix{}) ? nullptr : &vertexTransform;
  auto normalTransformX =
      (normalTransform == fbxsdk::FbxMatrix{}) ? nullptr : &normalTransform;

  std::optional<NodeMeshesSkinData> nodeMeshesSkinData;
  nodeMeshesSkinData = _extractNodeMeshesSkinData(fbx_meshes_);

  FbxNodeMeshesBumpMeta myMeta;
  if (_options.export_blend_shape) {
    myMeta.blendShapeMeta = _extractNodeMeshesBlendShape(fbx_meshes_);
  }

  std::optional<std::uint32_t> glTFSkinIndex;

  fx::gltf::Mesh glTFMesh;
  glTFMesh.name = meshName;

  for (decltype(fbx_meshes_.size()) iFbxMesh = 0; iFbxMesh < fbx_meshes_.size();
       ++iFbxMesh) {
    const auto fbxMesh = fbx_meshes_[iFbxMesh];

    std::vector<fbxsdk::FbxShape *> fbxShapes;
    if (myMeta.blendShapeMeta) {
      fbxShapes = myMeta.blendShapeMeta->blendShapeDatas[iFbxMesh].getShapes();
    }

    std::span<MeshSkinData::InfluenceChannel> skinInfluenceChannels;
    if (nodeMeshesSkinData) {
      skinInfluenceChannels = nodeMeshesSkinData->meshChannels[iFbxMesh];
    }

    auto glTFPrimitive = _convertMeshAsPrimitive(
        *fbxMesh, meshName, vertexTransformX, normalTransformX, fbxShapes,
        skinInfluenceChannels);

    auto materialIndex = _getTheUniqueMaterialIndex(*fbxMesh);
    if (materialIndex >= 0) {
      const auto fbxMaterial = fbx_node_.GetMaterial(materialIndex);
      if (auto glTFMaterialIndex = _convertMaterial(*fbxMaterial)) {
        glTFPrimitive.material = *glTFMaterialIndex;
      }
    }

    glTFMesh.primitives.emplace_back(std::move(glTFPrimitive));
  }

  if (myMeta.blendShapeMeta &&
      !myMeta.blendShapeMeta->blendShapeDatas.empty()) {
    // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#morph-targets
    // > Implementation note: A significant number of authoring and client
    // implementations associate names with morph targets. > While the
    // glTF 2.0 specification currently does not provide a way to specify
    // names, most tools use an array of strings, >
    // mesh.extras.targetNames, for this purpose. The targetNames array
    // and all primitive targets arrays must have the same length.
    const auto fbxShapeNames =
        myMeta.blendShapeMeta->blendShapeDatas.front().getShapeNames();
    glTFMesh.extensionsAndExtras["extras"]["targetNames"] = fbxShapeNames;
  }

  const auto glTFMeshIndex =
      _glTFBuilder.add(&fx::gltf::Document::meshes, std::move(glTFMesh));

  ConvertMeshResult convertMeshResult;
  convertMeshResult.glTFMeshIndex = glTFMeshIndex;
  if (nodeMeshesSkinData) {
    const auto glTFSkinIndex = _createGLTFSkin(*nodeMeshesSkinData);
    convertMeshResult.glTFSkinIndex = glTFSkinIndex;
  }

  myMeta.meshes = fbx_meshes_;
  node_meta_.meshes = myMeta;

  return convertMeshResult;
}

std::string SceneConverter::_getName(fbxsdk::FbxMesh &fbx_mesh_,
                                     fbxsdk::FbxNode &fbx_node_) {
  auto meshName = std::string{fbx_mesh_.GetName()};
  if (!meshName.empty()) {
    return meshName;
  } else {
    return _getName(fbx_node_);
  }
}

std::tuple<fbxsdk::FbxMatrix, fbxsdk::FbxMatrix>
SceneConverter::_getGeometrixTransform(fbxsdk::FbxNode &fbx_node_) {
  const auto meshTranslation = fbx_node_.GetGeometricTranslation(
      fbxsdk::FbxNode::EPivotSet::eSourcePivot);
  const auto meshRotation =
      fbx_node_.GetGeometricRotation(fbxsdk::FbxNode::EPivotSet::eSourcePivot);
  const auto meshScaling =
      fbx_node_.GetGeometricScaling(fbxsdk::FbxNode::EPivotSet::eSourcePivot);
  const fbxsdk::FbxMatrix vertexTransform =
      fbxsdk::FbxAMatrix{meshTranslation, meshRotation, meshScaling};
  const fbxsdk::FbxMatrix normalTransform =
      fbxsdk::FbxAMatrix{fbxsdk::FbxVector4(), meshRotation, meshScaling};
  auto normalTransformIT = normalTransform.Inverse().Transpose();
  return {vertexTransform, normalTransformIT};
}

fx::gltf::Primitive SceneConverter::_convertMeshAsPrimitive(
    fbxsdk::FbxMesh &fbx_mesh_,
    std::string_view mesh_name_,
    fbxsdk::FbxMatrix *vertex_transform_,
    fbxsdk::FbxMatrix *normal_transform_,
    std::span<fbxsdk::FbxShape *> fbx_shapes_,
    std::span<MeshSkinData::InfluenceChannel> skin_influence_channels_) {
  auto vertexLayout =
      _getFbxMeshVertexLayout(fbx_mesh_, fbx_shapes_, skin_influence_channels_);
  const auto vertexSize = vertexLayout.size;

  using UniqueVertexIndex = std::uint32_t;

  UntypedVertexVector untypedVertexAllocator{vertexSize};
  std::unordered_map<UntypedVertex, UniqueVertexIndex, UntypedVertexHasher,
                     UntypedVertexEqual>
      uniqueVertices({}, 0, UntypedVertexHasher{},
                     UntypedVertexEqual{vertexSize});

  const auto nMeshPolygonVertices = fbx_mesh_.GetPolygonVertexCount();
  auto meshPolygonVertices = fbx_mesh_.GetPolygonVertices();
  auto controlPoints = fbx_mesh_.GetControlPoints();
  auto stagingVertex = untypedVertexAllocator.allocate();
  auto processPolygonVertex =
      [&](int polygon_vertex_index_) -> UniqueVertexIndex {
    auto [stagingVertexData, stagingVertexIndex] = stagingVertex;
    auto iControlPoint = meshPolygonVertices[polygon_vertex_index_];

    fbxsdk::FbxVector4 transformedBasePosition;
    fbxsdk::FbxVector4 transformedBaseNormal;

    // Position
    {
      auto position = controlPoints[iControlPoint];
      if (vertex_transform_) {
        position = vertex_transform_->MultNormalize(position);
      }
      transformedBasePosition = position;
      auto pPosition =
          reinterpret_cast<NeutralVertexComponent *>(stagingVertexData);
      FbxVec3Spreader::spread(position, pPosition);
    }

    // Normal
    if (vertexLayout.normal) {
      auto [offset, element] = *vertexLayout.normal;
      auto normal = element(iControlPoint, polygon_vertex_index_);
      if (normal_transform_) {
        normal = normal_transform_->MultNormalize(normal);
      }
      transformedBaseNormal = normal;
      auto pNormal = reinterpret_cast<NeutralNormalComponent *>(
          stagingVertexData + offset);
      FbxVec3Spreader::spread(normal, pNormal);
    }

    // UV
    for (auto [offset, element] : vertexLayout.uvs) {
      auto uv = element(iControlPoint, polygon_vertex_index_);
      if (!_options.noFlipV) {
        uv[1] = 1.0 - uv[1];
      }
      auto pUV =
          reinterpret_cast<NeutralUVComponent *>(stagingVertexData + offset);
      FbxVec2Spreader::spread(uv, pUV);
    }

    // Vertex color
    for (auto [offset, element] : vertexLayout.colors) {
      auto color = element(iControlPoint, polygon_vertex_index_);
      auto pColor = reinterpret_cast<NeutralVertexColorComponent *>(
          stagingVertexData + offset);
      FbxColorSpreader::spread(color, pColor);
    }

    // Skinning
    if (vertexLayout.skinning) {
      auto [nChannels, jointsOffset, weightsOffset] = *vertexLayout.skinning;
      auto pJoints = reinterpret_cast<NeutralVertexJointComponent *>(
          stagingVertexData + jointsOffset);
      auto pWeights = reinterpret_cast<NeutralVertexWeightComponent *>(
          stagingVertexData + weightsOffset);
      for (decltype(nChannels) iChannel = 0; iChannel < nChannels; ++iChannel) {
        pJoints[iChannel] =
            skin_influence_channels_[iChannel].joints[iControlPoint];
        pWeights[iChannel] =
            skin_influence_channels_[iChannel].weights[iControlPoint];
      }
    }

    // Shapes
    for (auto &[controlPoints, normalElement] : vertexLayout.shapes) {
      {
        auto shapePosition = controlPoints.element[iControlPoint];
        if (vertex_transform_) {
          shapePosition = vertex_transform_->MultNormalize(shapePosition);
        }
        auto shapeDiff = shapePosition - transformedBasePosition;
        auto pPosition = reinterpret_cast<NeutralVertexComponent *>(
            stagingVertexData + controlPoints.offset);
        FbxVec3Spreader::spread(shapeDiff, pPosition);
      }

      if (normalElement && vertexLayout.normal) {
        auto [offset, element] = *normalElement;
        auto normal = element(iControlPoint, polygon_vertex_index_);
        if (normal_transform_) {
          normal = normal_transform_->MultNormalize(normal);
        }
        auto normalDiff = normal - transformedBaseNormal;
        auto pNormal = reinterpret_cast<NeutralNormalComponent *>(
            stagingVertexData + offset);
        FbxVec3Spreader::spread(normalDiff, pNormal);
      }
    }

    auto [rInserted, success] =
        uniqueVertices.try_emplace(stagingVertexData, stagingVertexIndex);
    auto [insertedVertex, uniqueVertexIndex] = *rInserted;
    if (success) {
      stagingVertex = untypedVertexAllocator.allocate();
    }

    return uniqueVertexIndex;
  };

  std::vector<UniqueVertexIndex> indices(nMeshPolygonVertices);
  for (std::remove_const_t<decltype(nMeshPolygonVertices)> iPolygonVertex = 0;
       iPolygonVertex < nMeshPolygonVertices; ++iPolygonVertex) {
    indices[iPolygonVertex] = processPolygonVertex(iPolygonVertex);
  }

  untypedVertexAllocator.pop_back();
  const auto nUniqueVertices = untypedVertexAllocator.size();
  auto uniqueVerticesData = untypedVertexAllocator.merge();

  // Debug blend shape data
  /*std::vector<std::vector<std::array<NeutralVertexComponent, 3>>>
      shapePositions;
  std::vector<std::vector<std::array<NeutralNormalComponent, 3>>>
      shapeNormals;
  if (!vertexLayout.shapes.empty()) {
    shapePositions.resize(nUniqueVertices);
    shapeNormals.resize(nUniqueVertices);
    for (std::remove_const_t<decltype(nUniqueVertices)> iVertex = 0;
         iVertex < nUniqueVertices; ++iVertex) {
      shapePositions[iVertex].resize(vertexLayout.shapes.size());
      shapeNormals[iVertex].resize(vertexLayout.shapes.size());
      for (int iShape = 0; iShape < vertexLayout.shapes.size(); ++iShape) {
        auto p = reinterpret_cast<NeutralVertexComponent *>(
            uniqueVerticesData.get() + vertexLayout.size * iVertex +
            vertexLayout.shapes[iShape].constrolPoints.offset);
        shapePositions[iVertex][iShape] = {p[0], p[1], p[2]};
        if (vertexLayout.shapes[iShape].normal) {
          auto n = reinterpret_cast<NeutralNormalComponent *>(
              uniqueVerticesData.get() + vertexLayout.size * iVertex +
              vertexLayout.shapes[iShape].normal->offset);
          shapeNormals[iVertex][iShape] = {n[0], n[1], n[2]};
        }
      }
    }
  }*/

  auto bulks = _typeVertices(vertexLayout);
  auto glTFPrimitive = _createPrimitive(
      bulks, static_cast<std::uint32_t>(fbx_shapes_.size()), nUniqueVertices,
      uniqueVerticesData.get(), vertexLayout.size, indices, mesh_name_);

  return glTFPrimitive;
}

FbxMeshVertexLayout SceneConverter::_getFbxMeshVertexLayout(
    fbxsdk::FbxMesh &fbx_mesh_,
    std::span<fbxsdk::FbxShape *> fbx_shapes_,
    std::span<MeshSkinData::InfluenceChannel> skin_influence_channels_) {
  FbxMeshVertexLayout vertexLaytout;

  auto normalElement0 = fbx_mesh_.GetElementNormal(0);
  if (normalElement0) {
    vertexLaytout.normal.emplace(vertexLaytout.size,
                                 makeFbxLayerElementAccessor(*normalElement0));
    vertexLaytout.size += sizeof(NeutralNormalComponent) * 3;
  }

  {
    auto nUVElements = fbx_mesh_.GetElementUVCount();
    if (nUVElements) {
      vertexLaytout.uvs.resize(nUVElements);
      for (decltype(nUVElements) iUVElement = 0; iUVElement < nUVElements;
           ++iUVElement) {
        vertexLaytout.uvs[iUVElement] = {
            vertexLaytout.size,
            makeFbxLayerElementAccessor(*fbx_mesh_.GetElementUV(iUVElement))};
        vertexLaytout.size += sizeof(NeutralUVComponent) * 2;
      }
    }
  }

  {
    auto nVertexColorElements = fbx_mesh_.GetElementVertexColorCount();
    if (nVertexColorElements) {
      vertexLaytout.colors.resize(nVertexColorElements);
      for (decltype(nVertexColorElements) iVertexColorElement = 0;
           iVertexColorElement < nVertexColorElements; ++iVertexColorElement) {
        vertexLaytout.colors[iVertexColorElement] = {
            vertexLaytout.size,
            makeFbxLayerElementAccessor(
                *fbx_mesh_.GetElementVertexColor(iVertexColorElement))};
        vertexLaytout.size += sizeof(NeutralVertexColorComponent) * 4;
      }
    }
  }

  if (!skin_influence_channels_.empty()) {
    vertexLaytout.skinning.emplace();

    const auto nChannels =
        static_cast<std::uint32_t>(skin_influence_channels_.size());
    vertexLaytout.skinning->channelCount =
        static_cast<std::uint32_t>(nChannels);

    vertexLaytout.skinning->joints = vertexLaytout.size;
    vertexLaytout.size += sizeof(NeutralVertexJointComponent) * nChannels;

    vertexLaytout.skinning->weights = vertexLaytout.size;
    vertexLaytout.size += sizeof(NeutralVertexWeightComponent) * nChannels;
  }

  vertexLaytout.shapes.reserve(fbx_shapes_.size());
  for (std::remove_cv_t<decltype(fbx_shapes_.size())> iShape = 0;
       iShape < fbx_shapes_.size(); ++iShape) {
    auto fbxShape = fbx_shapes_[iShape];
    FbxMeshVertexLayout::ShapeLayout shapeLayout;

    shapeLayout.constrolPoints = {vertexLaytout.size,
                                  fbxShape->GetControlPoints()};
    vertexLaytout.size += sizeof(NeutralVertexComponent) * 3;

    if (auto normalLayer = fbxShape->GetElementNormal()) {
      shapeLayout.normal.emplace(vertexLaytout.size,
                                 makeFbxLayerElementAccessor(*normalLayer));
      vertexLaytout.size += sizeof(NeutralNormalComponent) * 3;
    }
    vertexLaytout.shapes.emplace_back(std::move(shapeLayout));
  }

  return vertexLaytout;
}

fx::gltf::Primitive
SceneConverter::_createPrimitive(std::list<VertexBulk> &bulks_,
                                 std::uint32_t target_count_,
                                 std::uint32_t vertex_count_,
                                 std::byte *untyped_vertices_,
                                 std::uint32_t vertex_size_,
                                 std::span<std::uint32_t> indices_,
                                 std::string_view primitive_name_) {
  fx::gltf::Primitive glTFPrimitive;
  glTFPrimitive.targets.resize(target_count_);

  for (const auto &bulk : bulks_) {
    auto [bufferViewData, bufferViewIndex] =
        _glTFBuilder.createBufferView(bulk.stride * vertex_count_, 0, 0);
    auto &glTFBufferView =
        _glTFBuilder.get(&fx::gltf::Document::bufferViews)[bufferViewIndex];
    if (bulk.morphTargetHint) {
      glTFBufferView.name =
          fmt::format("{}/Target-{}", primitive_name_, *bulk.morphTargetHint);
    } else {
      glTFBufferView.name =
          fmt::format("{}", primitive_name_, *bulk.morphTargetHint);
    }
    if (bulk.vertexBuffer) {
      glTFBufferView.target = fx::gltf::BufferView::TargetType::ArrayBuffer;
    }
    glTFBufferView.byteStride = bulk.stride;

    for (const auto &channel : bulk.channels) {
      for (decltype(vertex_count_) iVertex = 0; iVertex < vertex_count_;
           ++iVertex) {
        channel.writer(
            bufferViewData + bulk.stride * iVertex + channel.outOffset,
            untyped_vertices_ + vertex_size_ * iVertex + channel.inOffset);
      }

      fx::gltf::Accessor glTFAccessor;
      glTFAccessor.name = fmt::format(
          "{0}{1}/{2}", primitive_name_,
          channel.target ? fmt::format("/Target-{}", *channel.target) : "",
          channel.name);
      glTFAccessor.bufferView = bufferViewIndex;
      glTFAccessor.byteOffset = channel.outOffset;
      glTFAccessor.count = vertex_count_;
      glTFAccessor.type = channel.type;
      glTFAccessor.componentType = channel.componentType;

      if (channel.name == "POSITION") {
        std::array<NeutralVertexComponent, 3> minPos, maxPos;
        std::fill(minPos.begin(), minPos.end(),
                  std::numeric_limits<NeutralVertexComponent>::infinity());
        std::fill(maxPos.begin(), maxPos.end(),
                  -std::numeric_limits<NeutralVertexComponent>::infinity());
        for (decltype(vertex_count_) iVertex = 0; iVertex < vertex_count_;
             ++iVertex) {
          auto pPosition = reinterpret_cast<const NeutralVertexComponent *>(
              untyped_vertices_ + vertex_size_ * iVertex + channel.inOffset);
          for (auto i = 0; i < 3; ++i) {
            minPos[i] = std::min(pPosition[i], minPos[i]);
            maxPos[i] = std::max(pPosition[i], maxPos[i]);
          }
        }
        auto typeCast = [](NeutralVertexComponent v_) {
          return static_cast<float>(v_);
        };
        glTFAccessor.min.resize(minPos.size());
        std::transform(minPos.begin(), minPos.end(), glTFAccessor.min.begin(),
                       typeCast);
        glTFAccessor.max.resize(maxPos.size());
        std::transform(maxPos.begin(), maxPos.end(), glTFAccessor.max.begin(),
                       typeCast);
      }

      auto glTFAccessorIndex = _glTFBuilder.add(&fx::gltf::Document::accessors,
                                                std::move(glTFAccessor));

      if (!channel.target) {
        glTFPrimitive.attributes.emplace(channel.name, glTFAccessorIndex);
      } else {
        const auto targetIndex = *channel.target;
        auto &target = glTFPrimitive.targets[targetIndex];
        target.emplace(channel.name, glTFAccessorIndex);
      }
    }
  }

  {
    using IndexUnit = GLTFComponentTypeStorage<
        fx::gltf::Accessor::ComponentType::UnsignedInt>;
    auto [bufferViewData, bufferViewIndex] = _glTFBuilder.createBufferView(
        static_cast<std::uint32_t>(indices_.size_bytes()), 0, 0);
    std::memcpy(bufferViewData, indices_.data(), indices_.size_bytes());
    auto &glTFBufferView =
        _glTFBuilder.get(&fx::gltf::Document::bufferViews)[bufferViewIndex];

    fx::gltf::Accessor glTFAccessor;
    glTFAccessor.name = fmt::format("{0}/INDICES", primitive_name_);
    glTFAccessor.bufferView = bufferViewIndex;
    glTFAccessor.count = static_cast<std::uint32_t>(indices_.size());
    glTFAccessor.type = fx::gltf::Accessor::Type::Scalar;
    glTFAccessor.componentType = fx::gltf::Accessor::ComponentType::UnsignedInt;

    auto glTFAccessorIndex = _glTFBuilder.add(&fx::gltf::Document::accessors,
                                              std::move(glTFAccessor));
    glTFPrimitive.indices = glTFAccessorIndex;
  }

  return glTFPrimitive;
}

std::list<SceneConverter::VertexBulk>
SceneConverter::_typeVertices(const FbxMeshVertexLayout &vertex_layout_) {
  std::list<VertexBulk> bulks;

  auto &defaultBulk = bulks.emplace_back();
  defaultBulk.vertexBuffer = true;

  {
    defaultBulk.addChannel(
        "POSITION",                               // name
        fx::gltf::Accessor::Type::Vec3,           // type
        fx::gltf::Accessor::ComponentType::Float, // component type
        0,                                        // in offset
        untypedVertexCopy<
            GLTFComponentTypeStorage<fx::gltf::Accessor::ComponentType::Float>,
            NeutralVertexComponent, 3> // writer
    );
  }

  if (vertex_layout_.normal) {
    defaultBulk.addChannel(
        "NORMAL",                                 // name
        fx::gltf::Accessor::Type::Vec3,           // type
        fx::gltf::Accessor::ComponentType::Float, // component type
        vertex_layout_.normal->offset,            // in offset
        untypedVertexCopy<
            GLTFComponentTypeStorage<fx::gltf::Accessor::ComponentType::Float>,
            NeutralNormalComponent, 3> // writer
    );
  }

  {
    auto nUV = vertex_layout_.uvs.size();
    for (decltype(nUV) iUV = 0; iUV < nUV; ++iUV) {
      auto &uvLayout = vertex_layout_.uvs[iUV];
      defaultBulk.addChannel(
          "TEXCOORD_" + std::to_string(iUV),        // name
          fx::gltf::Accessor::Type::Vec2,           // type
          fx::gltf::Accessor::ComponentType::Float, // component type
          uvLayout.offset,                          // in offset
          untypedVertexCopy<GLTFComponentTypeStorage<
                                fx::gltf::Accessor::ComponentType::Float>,
                            NeutralUVComponent, 2> // writer
      );
    }
  }

  {
    auto nColor = vertex_layout_.colors.size();
    for (decltype(nColor) iColor = 0; iColor < nColor; ++iColor) {
      auto &colorLayout = vertex_layout_.colors[iColor];
      defaultBulk.addChannel(
          "COLOR_" + std::to_string(iColor),        // name
          fx::gltf::Accessor::Type::Vec4,           // type
          fx::gltf::Accessor::ComponentType::Float, // component type
          colorLayout.offset,                       // in offset
          untypedVertexCopy<GLTFComponentTypeStorage<
                                fx::gltf::Accessor::ComponentType::Float>,
                            NeutralVertexColorComponent, 4> // writer
      );
    }
  }

  if (vertex_layout_.skinning) {
    const auto &[channelCount, jointsOffset, weightsOffset] =
        *vertex_layout_.skinning;
    constexpr std::uint32_t setCapacity = 4;
    constexpr auto glTFType = fx::gltf::Accessor::Type::Vec4;
    const auto set = channelCount % setCapacity == 0
                         ? channelCount / setCapacity
                         : (channelCount / setCapacity + 1);
    for (std::remove_const_t<decltype(set)> iSet = 0; iSet < set; ++iSet) {
      const auto setElement =
          (iSet == set - 1) ? channelCount % setCapacity : setCapacity;
      defaultBulk.addChannel(
          "JOINTS_" + std::to_string(iSet),                 // name
          glTFType,                                         // type
          fx::gltf::Accessor::ComponentType::UnsignedShort, // component type
          jointsOffset + sizeof(NeutralVertexJointComponent) * setCapacity *
                             iSet, // in offset
          makeUntypedVertexCopyN<
              GLTFComponentTypeStorage<
                  fx::gltf::Accessor::ComponentType::UnsignedShort>,
              NeutralVertexJointComponent>(setElement) // writer
      );
      defaultBulk.addChannel(
          "WEIGHTS_" + std::to_string(iSet),        // name
          glTFType,                                 // type
          fx::gltf::Accessor::ComponentType::Float, // component type
          weightsOffset + sizeof(NeutralVertexWeightComponent) * setCapacity *
                              iSet, // in offset
          makeUntypedVertexCopyN<GLTFComponentTypeStorage<
                                     fx::gltf::Accessor::ComponentType::Float>,
                                 NeutralVertexWeightComponent>(
              setElement) // writer
      );
    }
  }

  for (decltype(vertex_layout_.shapes.size()) iShape = 0;
       iShape < vertex_layout_.shapes.size(); ++iShape) {
    auto &shape = vertex_layout_.shapes[iShape];
    auto &shapeBulk = bulks.emplace_back();
    shapeBulk.morphTargetHint = static_cast<GLTFBuilder::XXIndex>(iShape);

    {
      shapeBulk.addChannel(
          "POSITION",                               // name
          fx::gltf::Accessor::Type::Vec3,           // type
          fx::gltf::Accessor::ComponentType::Float, // component type
          shape.constrolPoints.offset,              // in offset
          untypedVertexCopy<GLTFComponentTypeStorage<
                                fx::gltf::Accessor::ComponentType::Float>,
                            NeutralVertexComponent, 3>, // writer
          static_cast<std::uint32_t>(iShape)            // target index
      );
    }

    if (shape.normal) {
      shapeBulk.addChannel(
          "NORMAL",                                 // name
          fx::gltf::Accessor::Type::Vec3,           // type
          fx::gltf::Accessor::ComponentType::Float, // component type
          shape.normal->offset,                     // in offset
          untypedVertexCopy<GLTFComponentTypeStorage<
                                fx::gltf::Accessor::ComponentType::Float>,
                            NeutralNormalComponent, 3>, // writer
          static_cast<std::uint32_t>(iShape)            // target index
      );
    }
  }

  return bulks;
}

int SceneConverter::_getTheUniqueMaterialIndex(fbxsdk::FbxMesh &fbx_mesh_) {
  const auto nElementMaterialCount = fbx_mesh_.GetElementMaterialCount();
  if (!nElementMaterialCount) {
    return -1;
  }
  if (nElementMaterialCount > 1) {
    _log(Logger::Level::warning,
         MultiMaterialLayersError{fbx_mesh_.GetNode()->GetName()});
  }
  auto elementMaterial0 = fbx_mesh_.GetElementMaterial(0);
  if (elementMaterial0->GetMappingMode() != fbxsdk::FbxLayerElement::eAllSame) {
    throw std::runtime_error("Mesh is not splitted correctly!");
  }
  auto &indexArray = elementMaterial0->GetIndexArray();
  assert(indexArray.GetCount());
  return indexArray.GetAt(0);
}
} // namespace bee