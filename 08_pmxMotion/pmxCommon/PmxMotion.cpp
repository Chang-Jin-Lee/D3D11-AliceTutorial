#include "PmxMotion.h"
#include <windows.h>
#include <algorithm>

using namespace DirectX;

namespace pmx
{
	std::wstring NormalizeBoneName(const std::wstring& in)
	{
		std::wstring s = in;
		for (auto& ch : s) if (ch == L'?') ch = L'?';
		if (s == L"Center") s = L"«»«ó«¿?";
		return s;
	}

	bool LoadVMD(const std::wstring& path, VmdTracks& outTracks, uint32_t& outMaxFrame)
	{
		outTracks.clear(); outMaxFrame = 0;
		FILE* fp=nullptr; _wfopen_s(&fp, path.c_str(), L"rb"); if(!fp) return false;
		auto rd=[&](void* p,size_t s){ return fread(p,1,s,fp)==s; };
		char header[30]={0}; if(!rd(header,30)){ fclose(fp); return false; }
		char model[20]={0}; if(!rd(model,20)){ fclose(fp); return false; }
		uint32_t boneCount=0; if(!rd(&boneCount,4)){ fclose(fp); return false; }
		if (boneCount>1000000){ fclose(fp); return false; }
		for (uint32_t i=0;i<boneCount;++i){
			char nameSJIS[15]={0}; if(!rd(nameSJIS,15)){ fclose(fp); return false; }
			uint32_t frame; if(!rd(&frame,4)){ fclose(fp); return false; }
			float px,py,pz; if(!rd(&px,4)||!rd(&py,4)||!rd(&pz,4)){ fclose(fp); return false; }
			float qx,qy,qz,qw; if(!rd(&qx,4)||!rd(&qy,4)||!rd(&qz,4)||!rd(&qw,4)){ fclose(fp); return false; }
			unsigned char bez[64]; if(!rd(bez,64)){ fclose(fp); return false; }
			wchar_t wname[64]={0}; MultiByteToWideChar(932,0,nameSJIS,-1,wname,63); wname[63]=0;
			std::wstring boneName = NormalizeBoneName(wname);
			VmdKey k; k.frame=frame; k.pos={px,py,pz}; k.rot={qx,qy,qz,qw};
			outTracks[boneName].push_back(k);
			if (frame>outMaxFrame) outMaxFrame=frame;
		}
		fclose(fp);
		for (auto& kv : outTracks){ auto& v=kv.second; std::sort(v.begin(), v.end(), [](const VmdKey&a,const VmdKey&b){return a.frame<b.frame;}); }
		return true;
	}

	void SampleTrack(const std::vector<VmdKey>& keys, float tFrame, XMFLOAT3& outP, XMFLOAT4& outQ)
	{
		if (keys.empty()){ outP={0,0,0}; outQ={0,0,0,1}; return; }
		if (tFrame<=keys.front().frame){ outP=keys.front().pos; outQ=keys.front().rot; return; }
		if (tFrame>=keys.back().frame){ outP=keys.back().pos; outQ=keys.back().rot; return; }
		size_t hi=1; while(hi<keys.size() && keys[hi].frame<tFrame) ++hi; size_t lo=hi-1;
		float f0=(float)keys[lo].frame, f1=(float)keys[hi].frame; float u=(tFrame-f0)/(f1-f0);
		XMVECTOR p0=XMLoadFloat3(&keys[lo].pos), p1=XMLoadFloat3(&keys[hi].pos);
		XMVECTOR q0=XMLoadFloat4(&keys[lo].rot), q1=XMLoadFloat4(&keys[hi].rot);
		XMVECTOR p=XMVectorLerp(p0,p1,u); XMVECTOR q=XMQuaternionSlerp(q0,q1,u);
		XMStoreFloat3(&outP,p); XMStoreFloat4(&outQ,q);
	}
} 