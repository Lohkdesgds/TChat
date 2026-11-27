#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <battery/embed.hpp>

#include <array>
#include <string>
#include <string_view>
#include <atomic>
#include <thread>
#include <condition_variable>

constexpr size_t c_display_message_buf_max = 100;
constexpr float c_font_size = 24.0f;

class Display {
public:
    struct message {
        std::string user;
        std::string msg;
        SDL_Color color;

        message(std::string_view usr, std::string_view msg, std::string_view clr);
        message() = default;
    };

    Display();
    ~Display();

    std::string put_request(std::string_view title, std::string prefill = {});

    void push(message&& msg);

    bool is_closed() const;
    void destroy();

    bool has_touched();

    SDL_AppResult draw();
    SDL_AppResult handle(SDL_Event& ev);
private:
    void show_kb();
    void hide_kb();

    void setup_self();
    void destroy_self();

    SDL_Point get_window_size();
    SDL_Point get_text_size(std::string_view str);
    void draw_text(std::string_view str, float x, float y, SDL_Color clr, int max_w);

    const b::EmbedInternal::EmbeddedFile m_ttf_src;

    SDL_Renderer* m_rend{};
    SDL_Window* m_wind{};

    TTF_Font* m_font{};
    TTF_TextEngine* m_text_eng{};
    TTF_Text* m_text{};

    std::array<message, c_display_message_buf_max> m_messages;
    std::atomic_size_t m_message_offset{0};

    std::condition_variable m_cond_input;
    std::condition_variable m_cond;

    std::atomic_bool m_draw_new = true;

    std::string m_input_title;
    std::string m_input;
    bool m_read_input = false;

    std::atomic_bool m_thread_ready{false};
    bool m_closed = true;

    bool m_touched = false;
};