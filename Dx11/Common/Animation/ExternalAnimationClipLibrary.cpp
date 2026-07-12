#include "pch.h"
#include "ExternalAnimationClipLibrary.h"

#include "../Helper.h"

#include <assimp/anim.h>
#include <assimp/config.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

ExternalAnimationClipLibrary::ClipEntry::ClipEntry() = default;

ExternalAnimationClipLibrary::ClipEntry::~ClipEntry() = default;

ExternalAnimationClipLibrary::ClipEntry::ClipEntry(ClipEntry&& other) noexcept = default;

ExternalAnimationClipLibrary::ClipEntry& ExternalAnimationClipLibrary::ClipEntry::operator=(ClipEntry&& other) noexcept = default;

ExternalAnimationClipLibrary::~ExternalAnimationClipLibrary() = default;

ExternalAnimationClipLibrary::ExternalAnimationClipLibrary(ExternalAnimationClipLibrary&& other) noexcept = default;

ExternalAnimationClipLibrary& ExternalAnimationClipLibrary::operator=(ExternalAnimationClipLibrary&& other) noexcept = default;

namespace
{
    aiQuaternion ConvertUnrealRotationToGlb(const aiQuaternion& rotation)
    {
        const float halfSqrt = 0.7071067811865476f;
        const aiQuaternion basis(halfSqrt, halfSqrt, 0.0f, 0.0f);
        aiQuaternion inverseBasis = basis;
        inverseBasis.Conjugate();

        aiQuaternion converted = basis * rotation * inverseBasis;
        converted.Normalize();
        return converted;
    }

    void ApplyClipTransform(aiAnimation* animation, ExternalAnimationClipTransform transform)
    {
        if (!animation || transform == ExternalAnimationClipTransform::None)
            return;

        if (transform == ExternalAnimationClipTransform::UnrealCmZUpToGlbMeters)
        {
            for (unsigned channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
            {
                aiNodeAnim* channel = animation->mChannels[channelIndex];
                if (!channel)
                    continue;

                for (unsigned keyIndex = 0; keyIndex < channel->mNumPositionKeys; ++keyIndex)
                {
                    aiVector3D& value = channel->mPositionKeys[keyIndex].mValue;
                    const aiVector3D original = value;
                    value.x = original.x * 0.01f;
                    value.y = -original.z * 0.01f;
                    value.z = original.y * 0.01f;
                }

                for (unsigned keyIndex = 0; keyIndex < channel->mNumRotationKeys; ++keyIndex)
                {
                    channel->mRotationKeys[keyIndex].mValue =
                        ConvertUnrealRotationToGlb(channel->mRotationKeys[keyIndex].mValue);
                }
            }
        }
    }
}

bool ExternalAnimationClipLibrary::LoadClip(
    const std::string& key,
    const std::wstring& pathW,
    std::string* errorOut,
    ExternalAnimationClipTransform transform)
{
    if (key.empty())
    {
        if (errorOut) *errorOut = "empty animation key";
        return false;
    }

    auto importer = std::make_unique<Assimp::Importer>();
    importer->SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    importer->SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

    const unsigned flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ConvertToLeftHanded |
        aiProcess_LimitBoneWeights |
        aiProcess_ValidateDataStructure;

    const std::string pathUtf8 = Utf8FromWString(pathW);
    const aiScene* scene = importer->ReadFile(pathUtf8, flags);
    if (!scene)
    {
        if (errorOut) *errorOut = importer->GetErrorString();
        return false;
    }
    if (scene->mNumAnimations == 0 || !scene->mAnimations || !scene->mAnimations[0])
    {
        if (errorOut) *errorOut = "file contains no animation";
        return false;
    }
    ApplyClipTransform(scene->mAnimations[0], transform);

    ClipEntry entry;
    entry.pathW = pathW;
    entry.importer = std::move(importer);
    entry.scene = scene;
    entry.animationIndex = 0;
    m_Clips[key] = std::move(entry);
    return true;
}

void ExternalAnimationClipLibrary::Clear()
{
    m_Clips.clear();
}

const aiAnimation* ExternalAnimationClipLibrary::Get(const std::string& key) const
{
    const auto it = m_Clips.find(key);
    if (it == m_Clips.end()) return nullptr;

    const ClipEntry& entry = it->second;
    if (!entry.scene || entry.animationIndex >= entry.scene->mNumAnimations) return nullptr;
    return entry.scene->mAnimations[entry.animationIndex];
}

bool ExternalAnimationClipLibrary::Has(const std::string& key) const
{
    return Get(key) != nullptr;
}

std::vector<std::string> ExternalAnimationClipLibrary::Keys() const
{
    std::vector<std::string> keys;
    keys.reserve(m_Clips.size());
    for (const auto& pair : m_Clips)
    {
        keys.push_back(pair.first);
    }
    return keys;
}
