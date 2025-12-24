#pragma once
#include <cassert>

/*
* @brief Singleton 클래스.
* @details 이 클래스를 상속받은 클래스는 싱글톤 패턴을 적용할 수 있습니다.
*/

template <typename T>
class Singleton {
public:
	Singleton()
	{
		assert(s_instance == nullptr && "Singleton instance already created!");
		s_instance = static_cast<T*>(this);
	}

	virtual ~Singleton() = default;

	static void Create()
	{
		if (!s_instance)
			s_instance = new T();
	}
	// 명시적인 인스턴스 파괴
	static void Destroy()
	{
		delete s_instance;
		s_instance = nullptr;
	}

	// 복사 및 이동 금지
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(Singleton&&) = delete;

	static T& GetInstance()
	{
		assert(s_instance != nullptr && "Singleton instance not created!");
		return *s_instance;
	}
private:
	static T* s_instance;
};

template <typename T>
T* Singleton<T>::s_instance = nullptr;

#define GetSingleton(type) Singleton<type>::Get()