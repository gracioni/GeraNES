#pragma once
#include <SDL.h>

class SdlCursor
{
private:
    SDL_Cursor* m_cursor = nullptr;
    bool m_owned = true;  // controls whether SDL_FreeCursor should be called

    // Tracks the currently active cursor
    inline static SDL_Cursor* s_currentCursor = nullptr;

public:
    // Empty constructor (no cursor)
    SdlCursor() = default;

    // Construct from an existing cursor (optionally not owned)
    SdlCursor(SDL_Cursor* cursor, bool owned = true)
        : m_cursor(cursor), m_owned(owned)
    {}

    // Copy is disabled (we do not want multiple owners freeing the same cursor)
    SdlCursor(const SdlCursor&) = delete;
    SdlCursor& operator=(const SdlCursor&) = delete;

    // Move constructor
    SdlCursor(SdlCursor&& other) noexcept
    {
        m_cursor = other.m_cursor;
        m_owned  = other.m_owned;
        other.m_cursor = nullptr;
    }

    // Move assignment
    SdlCursor& operator=(SdlCursor&& other) noexcept
    {
        if (this != &other) {
            reset();  // free current cursor if owned
            m_cursor = other.m_cursor;
            m_owned  = other.m_owned;
            other.m_cursor = nullptr;
        }
        return *this;
    }

    // Destructor: automatically frees if owned == true
    ~SdlCursor()
    {
        reset();
    }

    // Frees the cursor if owned
    void reset()
    {
        if (m_cursor && m_owned) {
            SDL_FreeCursor(m_cursor);
        }
        m_cursor = nullptr;
        m_owned = true;
    }

    SDL_Cursor* get() const { return m_cursor; }

    // Sets this cursor as the active one
    void setAsCurrent() const
    {
        if (m_cursor) {
            SDL_SetCursor(m_cursor);
            s_currentCursor = m_cursor;
        }
    }

    bool isCurrent() const
    {
        return m_cursor == s_currentCursor;
    }

    // Safely create a system cursor
    static SdlCursor createSystemCursor(SDL_SystemCursor type)
    {
        SDL_Cursor* cur = SDL_CreateSystemCursor(type);
        return SdlCursor(cur, true);
    }

    // Safely create a color cursor
    static SdlCursor createColorCursor(SDL_Surface* surface, int hotX, int hotY)
    {
        SDL_Cursor* cur = SDL_CreateColorCursor(surface, hotX, hotY);
        return SdlCursor(cur, true);
    }

    // Get default cursor (should NOT be freed)
    static SdlCursor getDefault()
    {
        SDL_Cursor* cur = SDL_GetDefaultCursor();
        return SdlCursor(cur, false);  // not owned
    }
};
