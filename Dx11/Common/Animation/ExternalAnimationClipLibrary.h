#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct aiAnimation;
struct aiScene;

namespace Assimp
{
    class Importer;
}

class ExternalAnimationClipLibrary
{
public:
    ExternalAnimationClipLibrary() = default;
    ~ExternalAnimationClipLibrary();

    ExternalAnimationClipLibrary(ExternalAnimationClipLibrary&& other) noexcept;
    ExternalAnimationClipLibrary& operator=(ExternalAnimationClipLibrary&& other) noexcept;

    ExternalAnimationClipLibrary(const ExternalAnimationClipLibrary&) = delete;
    ExternalAnimationClipLibrary& operator=(const ExternalAnimationClipLibrary&) = delete;

    bool LoadClip(const std::string& key, const std::wstring& pathW, std::string* errorOut = nullptr);
    void Clear();

    const aiAnimation* Get(const std::string& key) const;
    bool Has(const std::string& key) const;
    std::vector<std::string> Keys() const;

private:
    struct ClipEntry
    {
        ClipEntry();
        ~ClipEntry();

        ClipEntry(ClipEntry&& other) noexcept;
        ClipEntry& operator=(ClipEntry&& other) noexcept;

        ClipEntry(const ClipEntry&) = delete;
        ClipEntry& operator=(const ClipEntry&) = delete;

        std::wstring pathW;
        std::unique_ptr<Assimp::Importer> importer;
        const aiScene* scene = nullptr;
        unsigned animationIndex = 0;
    };

    std::unordered_map<std::string, ClipEntry> m_Clips;
};
