// Singleton.h: interface for the Singleton class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

template <class T> 
class Singleton {
private:
	static T* pObject;
protected:
	Singleton() {}
	virtual ~Singleton() {}

public:
	static T* getInstance() {
		if (!pObject)
			pObject = new T();
		return pObject;
	}
	static void destroyInstance() {
		delete pObject;
		pObject = NULL;
	}
};

template <class T> 
T* Singleton<T>::pObject = NULL;

