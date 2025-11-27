#ifndef NBT_SERIALIZERS_HPP
#define NBT_SERIALIZERS_HPP

#include <nbt/nbt.hpp>
#include <math/vec3f.hpp>

inline NBTTagList *serialize_Vec3f(Vec3f vec)
{
    NBTTagList *result = new NBTTagList;
    result->addTag(new NBTTagDouble(vec.x));
    result->addTag(new NBTTagDouble(vec.y));
    result->addTag(new NBTTagDouble(vec.z));
    return result;
}

inline NBTTagList *serialize_Vec3i(Vec3i vec)
{
    NBTTagList *result = new NBTTagList;
    result->addTag(new NBTTagInt(vec.x));
    result->addTag(new NBTTagInt(vec.y));
    result->addTag(new NBTTagInt(vec.z));
    return result;
}

inline NBTTagList *serialize_Vec2f(Vec3f vec)
{
    NBTTagList *result = new NBTTagList;
    result->addTag(new NBTTagFloat(vec.x));
    result->addTag(new NBTTagFloat(vec.y));
    return result;
}

inline Vec3f deserialize_Vec3f(NBTTagList *list)
{
    if (!list || list->tagType != NBTBase::TAG_Double || list->value.size() < 3)
        return Vec3f(0, 0, 0);
    Vec3f result;
    result.x = ((NBTTagDouble *)list->getTag(0))->value;
    result.y = ((NBTTagDouble *)list->getTag(1))->value;
    result.z = ((NBTTagDouble *)list->getTag(2))->value;
    return result;
}

inline Vec3i deserialize_Vec3i(NBTTagList *list)
{
    if (!list || list->tagType != NBTBase::TAG_Int || list->value.size() < 3)
        return Vec3i(0, 0, 0);
    Vec3i result;
    result.x = ((NBTTagInt *)list->getTag(0))->value;
    result.y = ((NBTTagInt *)list->getTag(1))->value;
    result.z = ((NBTTagInt *)list->getTag(2))->value;
    return result;
}

inline Vec3f deserialize_Vec2f(NBTTagList *list)
{
    if (!list || list->tagType != NBTBase::TAG_Float || list->value.size() < 2)
        return Vec3f(0, 0, 0);
    Vec3f result;
    result.x = ((NBTTagFloat *)list->getTag(0))->value;
    result.y = ((NBTTagFloat *)list->getTag(1))->value;
    return result;
}

#endif