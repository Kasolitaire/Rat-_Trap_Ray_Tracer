#pragma once
class RenderObject
{
public:
	RenderObject() { assert(0); };
	RenderObject(unsigned int instanceIndex, unsigned int modelIndex) :
		instanceIndex(instanceIndex), modelIndex(modelIndex) {};
	unsigned int instanceIndex = 0;
	unsigned int modelIndex = 0;
private:

};

