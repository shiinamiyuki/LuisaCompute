#pragma once
#include <Common/GFXUtil.h>
#include <util/VObject.h>
class MeshRenderer;
class RendererCull
{
private:
	HashMap<uint2, ArrayList<MeshRenderer*>> allRendererReferences;
public:
	void RemoveRenderer(MeshRenderer* renderer);
	void AddRenderer(MeshRenderer* renderer);
};
