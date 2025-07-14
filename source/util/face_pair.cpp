#include "face_pair.hpp"
// Store the 15 possible unordered face pairs
static std::vector<std::pair<int, int>> face_pairs;

// Initialize the face_pairs vector
void init_face_pairs()
{
    if (!face_pairs.empty())
    {
        // Already initialized
        return;
    }
    for (int i = 0; i < 6; i++)
    {
        for (int j = i + 1; j < 6; j++)
        {
            face_pairs.emplace_back(i, j);
        }
    }
}
// Convert a face pair to a unique flag
uint16_t face_pair_to_flag(int face_a, int face_b)
{
    if (face_a == face_b)
        return 0; // Ignore same-face pairs

    int a = std::min(face_a, face_b);
    int b = std::max(face_a, face_b);

    for (size_t i = 0; i < face_pairs.size(); ++i)
    {
        if (face_pairs[i].first == a && face_pairs[i].second == b)
        {
            return 1 << i;
        }
    }
    return 0; // Not found (shouldn't happen if input is valid)
}