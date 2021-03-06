
#include <graphics/3d/tangent_calculator.h>
#include "gltf_model_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#include "../../external/stb/stb_image.h"
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "../../external/tiny_gltf.h"

GltfModelLoader::GltfModelLoader(const VertAttributes &attrs) : vertAttributes(attrs)
{

}

int componentTypeSize(int componentType)
{
    switch (componentType)
    {
        case GL_FLOAT:
            assert(sizeof(int32) == sizeof(float));
        case GL_INT:
        case GL_UNSIGNED_INT:
            return sizeof(int32);
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return sizeof(int16);
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return sizeof(int8);
    }
    throw gu_err("Error while loading glTF: " + std::to_string(componentType) + " not recognized as a component type (GL_FLOAT, GL_BYTE, etc..)");
}

int nrOfVertices(const tinygltf::Model &tiny, const tinygltf::Mesh &tinyMesh)
{
    int nrOfVerts = 0;
    for (auto &primitive : tinyMesh.primitives)
    {
        for (auto &[attrName, accessorI] : primitive.attributes)
        {
            nrOfVerts += tiny.accessors.at(accessorI).count;
            break;
        }
    }
    return nrOfVerts;
}

void loadMeshes(GltfModelLoader &loader, const tinygltf::Model &tiny)
{
    for (auto &tinyMesh : tiny.meshes)
    {
        int nrOfVerts = nrOfVertices(tiny, tinyMesh);
        loader.meshes.push_back(std::make_shared<Mesh>(tinyMesh.name, nrOfVerts, loader.vertAttributes));
        auto &mesh = loader.meshes.back();

        int nrOfVertsLoaded = 0;

        for (auto &primitive : tinyMesh.primitives)
        {
            if (primitive.indices < 0)
                continue;

            auto &part = mesh->parts.emplace_back();
            part.mode = primitive.mode;

            // indices:
            {
                auto &accessor = tiny.accessors.at(primitive.indices);

                if (accessor.componentType != GL_UNSIGNED_SHORT)
                    throw gu_err("Error while loading glTF: only unsigned shorts are supported as indices type.");

                if (accessor.count > 0)
                {
                    part.indices.resize(accessor.count);

                    auto &bufferView = tiny.bufferViews.at(accessor.bufferView);
                    auto &buffer = tiny.buffers.at(bufferView.buffer);

                    if (buffer.data.size() < bufferView.byteOffset + bufferView.byteLength)
                        throw gu_err("Error while loading glTF: buffer is too small");

                    if (bufferView.byteLength != part.indices.size() * sizeof(unsigned short))
                        throw gu_err("Error while loading glTF: indices bufferView byteLength is incorrect");

                    memcpy(&part.indices[0], &buffer.data.at(bufferView.byteOffset), bufferView.byteLength);
                    for (auto &index : part.indices)
                        index += nrOfVertsLoaded;
                }
            }

            // vertices:
            int primitiveVerts = 0;
            for (auto &[attrName, accessorI] : primitive.attributes)
            {
                auto &accessor = tiny.accessors.at(accessorI);
                primitiveVerts = accessor.count;

                VertAttr attr { attrName };
                if (attrName == "TEXCOORD_0")
                    attr = VertAttributes::TEX_COORDS;
                else if (attrName == "COLOR_0")
                    attr = accessor.type == 3 ? VertAttributes::RGB : VertAttributes::RGBA;

                attr.type = accessor.componentType;
                attr.size = accessor.type;
                attr.byteSize = componentTypeSize(attr.type) * attr.size;
                attr.normalized = accessor.normalized;

                if (!loader.vertAttributes.contains(attr))
                    continue;

                auto &bufferView = tiny.bufferViews.at(accessor.bufferView);
                auto &buffer = tiny.buffers.at(bufferView.buffer);

                if (buffer.data.size() < bufferView.byteOffset + bufferView.byteLength)
                    throw gu_err("Error while loading glTF: buffer is too small");

                if (bufferView.byteLength != primitiveVerts * attr.byteSize)
                    throw gu_err("Error while loading glTF: vertices bufferView byteLength is incorrect");

                auto attrOffset = loader.vertAttributes.getOffset(attr);

                int tinyVertI = 0;
                for (int vertI = nrOfVertsLoaded; vertI < nrOfVertsLoaded + primitiveVerts; vertI++)
                {
                    for (int component = 0; component < attr.size; component++)
                    {
                        switch (attr.type)
                        {
                            case GL_FLOAT:
                                assert(sizeof(int32) == sizeof(float));
                            case GL_INT:
                            case GL_UNSIGNED_INT:

                                mesh->set(*((float *) &buffer.data.at(bufferView.byteOffset + tinyVertI * attr.byteSize + component * sizeof(float))), vertI, attrOffset + component * sizeof(float));
                                break;
                            case GL_SHORT:
                            case GL_UNSIGNED_SHORT:

                                mesh->set(*((short *) &buffer.data.at(bufferView.byteOffset + tinyVertI * attr.byteSize + component * sizeof(short))), vertI, attrOffset + component * sizeof(short));
                                break;
                            case GL_BYTE:
                            case GL_UNSIGNED_BYTE:

                                mesh->set(*((char *) &buffer.data.at(bufferView.byteOffset + tinyVertI * attr.byteSize + component * sizeof(char))), vertI, attrOffset + component * sizeof(char));
                                break;
                        }
                    }
                    tinyVertI++;
                }
            }
            nrOfVertsLoaded += primitiveVerts;
        }
        assert(nrOfVertsLoaded == nrOfVerts);
    }

    if (loader.calculateTangents && loader.vertAttributes.contains(VertAttributes::TANGENT))
    {
        for (auto &mesh : loader.meshes)
            for (int i = 0; i < mesh->parts.size(); i++)
                TangentCalculator::addTangentsToMesh(mesh, i);
    }
}

void loadModels(GltfModelLoader &loader, const tinygltf::Model &tiny)
{
    for (auto &node : tiny.nodes)
    {
        if (node.mesh < 0)
            continue;

        loader.models.push_back(std::make_shared<Model>(node.name));
        auto &model = loader.models.back();
        auto &mesh = loader.meshes.at(node.mesh);
        auto &tinyMesh = tiny.meshes.at(node.mesh);
        int partI = 0;
        for (auto &meshPart : mesh->parts)
        {
            auto &modelPart = model->parts.emplace_back();
            modelPart.mesh = mesh;
            modelPart.material = loader.materials.at(tinyMesh.primitives.at(partI).material);

            modelPart.meshPartIndex = partI++;
        }
    }
}

template<int length=3>
void stdVectorToGlmVec(const std::vector<double> &stdVector, vec<length, float, defaultp> &glmVec)
{
    for (int i = 0; i < length; i++)
        glmVec[i] = stdVector.at(i);
}

template<class TinyTextureInfo>
void loadImageData(const tinygltf::Model &tiny, const TinyTextureInfo &info, TextureAssetOrPtr &out)
{
    if (info.index < 0)
        return;

    auto &tinyTexture = tiny.textures.at(info.index);
    if (tinyTexture.source >= 0)
    {
        auto &tinyImage = tiny.images.at(tinyTexture.source);

        if (tinyImage.component <= 0 || tinyImage.component > 4)
            throw gu_err(tinyImage.name + " has " + std::to_string(tinyImage.component) + " channels!");

        if (tinyImage.pixel_type != GL_UNSIGNED_BYTE)
            throw gu_err("pixel data of " + tinyImage.name + " is not GL_UNSIGNED_BYTE!");

        GLenum format = std::vector<GLenum>{GL_R, GL_RG, GL_RGB, GL_RGBA}[tinyImage.component - 1];

        out.sharedTex = SharedTexture(new Texture(Texture::fromByteData(tinyImage.image.data(), format, tinyImage.width, tinyImage.height, GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR)));
    }
}

void loadMaterials(GltfModelLoader &loader, const tinygltf::Model &tiny)
{
    for (auto &tinyMaterial : tiny.materials)
    {
        loader.materials.push_back(std::make_shared<Material>());
        auto &material = loader.materials.back();
        material->name = tinyMaterial.name;
        material->doubleSided = tinyMaterial.doubleSided;
        material->roughness = tinyMaterial.pbrMetallicRoughness.roughnessFactor;
        material->metallic = tinyMaterial.pbrMetallicRoughness.metallicFactor;

        stdVectorToGlmVec(tinyMaterial.pbrMetallicRoughness.baseColorFactor, material->diffuse);
        stdVectorToGlmVec(tinyMaterial.emissiveFactor, material->emissive);

        if (loader.loadDiffuseTextures)
            loadImageData(tiny, tinyMaterial.pbrMetallicRoughness.baseColorTexture, material->diffuseTexture);
        if (loader.loadNormalMaps)
            loadImageData(tiny, tinyMaterial.normalTexture, material->normalMap);
        material->normalMapScale = tinyMaterial.normalTexture.scale;
        if (loader.loadMetallicRoughnessTextures)
            loadImageData(tiny, tinyMaterial.pbrMetallicRoughness.metallicRoughnessTexture, material->metallicRoughnessTexture);
        if (loader.loadEmissiveTextures)
            loadImageData(tiny, tinyMaterial.emissiveTexture, material->emissiveTexture);
        if (loader.loadAOTextures)
            loadImageData(tiny, tinyMaterial.occlusionTexture, material->aoTexture);
        material->aoTextureStrength = tinyMaterial.occlusionTexture.strength;
    }
}

void load(GltfModelLoader &loader, tinygltf::Model &tiny)
{
    loadMaterials(loader, tiny);
    loadMeshes(loader, tiny);
    loadModels(loader, tiny);
}

void GltfModelLoader::fromASCIIFile(const char *path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF ctx;
    std::string err, warn;
    bool success = ctx.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!err.empty())
        throw gu_err(err);
    if (!warn.empty())
        std::cerr << "Warning while loading " << path << ":\n" << warn << std::endl;
    if (!success)
        throw gu_err("Failed to parse glTF");

    load(*this, model);
}

void GltfModelLoader::fromBinaryFile(const char *path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF ctx;
    std::string err, warn;
    bool success = ctx.LoadBinaryFromFile(&model, &err, &warn, path);

    if (!err.empty())
        throw gu_err(err);
    if (!warn.empty())
        std::cerr << "Warning while loading " << path << ":\n" << warn << std::endl;
    if (!success)
        throw gu_err("Failed to parse glTF");

    load(*this, model);
}
