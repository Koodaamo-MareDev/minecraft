#ifndef _VEC3I_HPP_
#define _VEC3I_HPP_
class vec3f;  // Forward declaration

class vec3i {
public:
    int x = 0, y = 0, z = 0;
    // Constructors, member variables, and other member functions
    
    bool operator==(vec3i const& a) const;
    vec3i operator+(vec3i const& a) const;
    vec3i operator-(vec3i const& a) const;
    vec3f operator+(vec3f const& a) const;
    vec3f operator-(vec3f const& a) const;

private:
    // Member variables
};
#endif