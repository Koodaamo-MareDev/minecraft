#ifndef DISPLAYLIST_HPP
#define DISPLAYLIST_HPP

#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <new>
#include <ogc/gx.h>

namespace gertex
{
    struct __attribute__((packed)) Vertex
    {
        float x, y, z;
        union
        {
            uint8_t r;
            uint8_t i;
        };
        uint8_t g, b, a, nrm;
        float u, v;
    };

    struct __attribute__((packed)) Vertex16
    {
        int16_t x, y, z;
        union
        {
            uint8_t r;
            uint8_t i;
        };
        uint8_t g, b, a, nrm;
        float u, v;
    };

    template <typename T>
    struct DisplayList
    {
        uint8_t *ptr;
        uint8_t *buffer;
        uint32_t start_copy_len;
        uint32_t attrib_size;
        uint16_t max_vertices;

        DisplayList(uint16_t max_vertices, uint32_t attrib_size)
        {
            this->max_vertices = max_vertices;
            this->start_copy_len = attrib_size - 9;
            this->attrib_size = attrib_size;
            this->ptr = this->buffer = nullptr;
        }

        virtual void begin(uint8_t primitive)
        {
            if (!buffer)
                ptr = buffer = new (std::align_val_t(32)) uint8_t[(max_vertices * attrib_size + 32 + 31) & ~31]();
            *ptr = primitive; // OR with VTXFMT0-7 if needed.
            std::memcpy(&ptr[1], &max_vertices, 2);
            ptr += 3;
        }

        virtual void put(const T &t)
        {
            if (size() + attrib_size > capacity())
                throw std::overflow_error("DisplayList overflow");
            std::memcpy(ptr, &t, start_copy_len);
            std::memcpy(&ptr[start_copy_len], &t.nrm, 9);
            ptr += attrib_size;
        }

        // Resets the write pointer to the beginning of the underlying buffer
        virtual void rewind()
        {
            ptr = buffer;
        }

        // Returns the current number of bytes written to this DisplayList
        virtual size_t size()
        {
            return ptr - buffer;
        }

        virtual size_t aligned_size()
        {
            return (size() + 32 + 31) & ~31;
        }

        // Returns the total number of bytes that can be written to this DisplayList
        virtual size_t capacity()
        {
            return max_vertices * attrib_size;
        }

        // Returns the size of the underlying buffer
        virtual size_t buffer_capacity()
        {
            return (capacity() + 32 + 31) & ~31;
        }

        virtual uint8_t *build()
        {
            uint8_t *new_buffer = new (std::align_val_t(32)) uint8_t[aligned_size()]();
            std::memcpy(new_buffer, buffer, size());

            return new_buffer;
        }

        virtual ~DisplayList()
        {
            if (buffer)
                delete[] buffer;
        }
    };

    template <typename T>
    struct DisplayListDiscard : public DisplayList<T>
    {
        DisplayListDiscard(uint16_t max_vertices, uint32_t attrib_size) : DisplayList<T>(max_vertices, attrib_size) {}

        virtual void begin(uint8_t primitive) override {};
        virtual void put(const T &t) override {}
        virtual uint8_t *build() override { return nullptr; };
    };

    template <typename T>
    struct DisplayListPass : public DisplayList<T>
    {
        DisplayListPass(uint16_t max_vertices, uint32_t attrib_size) : DisplayList<T>(max_vertices, attrib_size) {}

        virtual void begin(uint8_t primitive) override
        {
            GX_Begin(primitive, GX_VTXFMT0, this->max_vertices);
        }
        virtual void put(const T &t) override = 0;
        virtual uint8_t *build() override { return nullptr; };
    };

    template <typename T, typename I>
    struct DisplayListBuffered : public I
    {
        DisplayListBuffered(uint16_t max_vertices, uint32_t attrib_size) : I(max_vertices, attrib_size) {}

        uint8_t *read_ptr;
        uint16_t vertices;

        virtual void begin(uint8_t primitive) override
        {
            DisplayList<T>::begin(primitive);
            this->read_ptr = this->buffer + 3;
            vertices = 0;
        }

        virtual void put(const T &t) override
        {
            DisplayList<T>::put(t);
            vertices++;
        }

        virtual void flush()
        {
            if (!vertices)
                return;
            // Write begin command
            GX_Begin(*this->buffer, GX_VTXFMT0, vertices);
            T t;
            while (this->read_ptr < this->ptr)
            {
                std::memcpy(&t, this->read_ptr, this->start_copy_len);
                std::memcpy(&t.nrm, &this->read_ptr[this->start_copy_len], 9);
                this->read_ptr += this->attrib_size;
                I::put(t);
            }
        }
    };

    struct DisplayListPassF : public DisplayListPass<Vertex>
    {
        DisplayListPassF(uint16_t max_vertices, uint32_t attrib_size) : DisplayListPass<Vertex>(max_vertices, attrib_size) {}

        virtual void put(const Vertex &t) override
        {
            wgPipe->F32 = t.x;
            wgPipe->F32 = t.y;
            wgPipe->F32 = t.z;

            int count = start_copy_len - 12;
            const uint8_t *p = &t.r;
            for (int i = 0; i < count; i++)
                wgPipe->U8 = *p++;

            wgPipe->U8 = t.nrm;
            wgPipe->F32 = t.u;
            wgPipe->F32 = t.v;
        }
    };

    struct DisplayListPass16 : public DisplayListPass<Vertex16>
    {
        DisplayListPass16(uint16_t max_vertices, uint32_t attrib_size) : DisplayListPass<Vertex16>(max_vertices, attrib_size) {}

        virtual void put(const Vertex16 &t) override
        {
            wgPipe->S16 = t.x;
            wgPipe->S16 = t.y;
            wgPipe->S16 = t.z;

            int count = start_copy_len - 6;
            const uint8_t *p = &t.r;
            for (int i = 0; i < count; i++)
                wgPipe->U8 = *p++;

            wgPipe->U8 = t.nrm;
            wgPipe->F32 = t.u;
            wgPipe->F32 = t.v;
        }
    };

    using DisplayListDiscard16 = DisplayListDiscard<Vertex16>;
    using DisplayListDiscardF = DisplayListDiscard<Vertex>;
    using DisplayListBuffered16 = DisplayListBuffered<Vertex16, DisplayListPass16>;
    using DisplayListBufferedF = DisplayListBuffered<Vertex, DisplayListPassF>;

    extern DisplayListPass16 *gx_pass_displist16;
    extern DisplayListPassF *gx_pass_displistf;
    extern DisplayListDiscard16 *gx_pass_displist16_discard;
    extern DisplayListDiscardF *gx_pass_displistf_discard;

    void init_pass_displists();
} // namespace gertex

#endif