#include "display.hpp"

#include <cstdint>
#include <charconv>
#include <string_view>
#include <utility>

constexpr SDL_Point c_default_app_size = {720, 1280};
constexpr SDL_WindowFlags c_default_app_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
#if defined(SDL_PLATFORM_ANDROID)
    | SDL_WINDOW_FULLSCREEN
#endif
;

static TTF_Font* load_font_from_mem(const char* data, const size_t len, float ptsize);

Display::message::message(std::string_view usr, std::string_view msg, std::string_view clr) :
    user(usr),
    msg(msg)
{
    const auto parseByte = [](std::string_view part) -> uint8_t {
        uint8_t value;
        auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), value, 16);
        if (ec != std::errc{} || ptr != part.data() + part.size())
            return 0;
        return value;
    };

    if (clr.size() == 7) {
        color = SDL_Color{
            .r = parseByte(clr.substr(1, 2)),
            .g = parseByte(clr.substr(3, 2)),
            .b = parseByte(clr.substr(5, 2)),
            .a = 255
        };
    } else {
        color = SDL_Color{
            .r = 0,
            .g = 0,
            .b = 0,
            .a = 255
        };
    }
}

Display::Display() :
    m_ttf_src(b::embed<"resources/calibri_fix.ttf">())
{
    setup_self();
}

Display::~Display() {
    destroy_self();
}

std::string Display::put_request(std::string_view title, std::string prefill) {
    m_input_title = title;
    m_input = prefill;
    m_read_input = true;
    
    std::mutex m;
    std::unique_lock l(m);

    m_draw_new = true;
    m_cond.notify_one();

    while(!m_cond_input.wait_for(l, std::chrono::seconds(1), [this]{ return !m_read_input; }));
    return m_input;
}

void Display::push(message&& msg) {
    m_messages[m_message_offset] = std::move(msg);

    if (m_message_offset + 1 >= c_display_message_buf_max) {
        m_message_offset = 0;
    }
    else {
        ++m_message_offset;
    }
    m_draw_new = true;
    m_cond.notify_one();
}

bool Display::is_closed() const {
    return m_closed;
}

void Display::destroy() {
    destroy_self();
}

bool Display::has_touched() {
    return std::exchange(m_touched, false);
}

SDL_AppResult Display::draw() {
    static std::mutex m;
    std::unique_lock l(m);

    if (!m_draw_new)
        m_cond.wait_for(l, std::chrono::milliseconds(500), [this]{ return m_draw_new.load(); });

    m_draw_new = false;

    if (m_read_input) {
        const auto [w, h] = get_window_size();

        SDL_SetRenderDrawColor(m_rend, 30, 30, 40, 255);
        SDL_RenderClear(m_rend);

        auto siz = get_text_size(m_input_title);
        draw_text(m_input_title, (w - siz.x) * 0.5f, 10.0f, {255,255,255,255}, w);

        SDL_FRect block_rect{
            .x = 5.0f,
            .y = 10.0f + c_font_size + 5.0f - 2.0f,
            .w = w - 10.0f,
            .h = c_font_size + 4.0f
        };

        SDL_SetRenderDrawColor(m_rend, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderFillRect(m_rend, &block_rect);

        siz = get_text_size(m_input);
        draw_text(m_input, (w - siz.x) * 0.5f, 10.0f + c_font_size + 5.0f, {255,255,255,255}, w);

        SDL_RenderPresent(m_rend);
    } 
    else { // default drawing
        SDL_SetRenderDrawColor(m_rend, 30, 30, 40, 255);
        SDL_RenderClear(m_rend);

        constexpr float c_div_msg = 3.0f;
        constexpr SDL_Color c_white{255,255,255,255};
        constexpr SDL_Color c_black{0,0,0,255};
        constexpr auto lighter = [](const SDL_Color c) {
            return SDL_Color {
                .r = static_cast<uint8_t>(127 + static_cast<int>(c.r) / 2),
                .g = static_cast<uint8_t>(127 + static_cast<int>(c.g) / 2),
                .b = static_cast<uint8_t>(127 + static_cast<int>(c.b) / 2),
                .a = c.a
            };
        };

        const auto [w, h] = get_window_size();

        SDL_FPoint draw_p{
            .x = 2.0f,
            .y = static_cast<float>(h)
        };

        for (size_t p = 0; p < c_display_message_buf_max && draw_p.y > (- c_font_size); ++p) {
            const size_t curr = (m_message_offset + c_display_message_buf_max - p - 1) % c_display_message_buf_max;
            const message& ref = m_messages[curr];
            if (ref.user.empty() || ref.msg.empty()) break;

            const auto msg_size = get_text_size(ref.msg);
            const auto usr_size = get_text_size(ref.user);

            draw_p.y -= msg_size.y;
            draw_text(ref.msg, draw_p.x + 1.0f, draw_p.y + 1.0f, c_black, w);
            draw_text(ref.msg, draw_p.x, draw_p.y, c_white, w);

            SDL_FRect block_rect{
                .x = draw_p.x,
                .y = draw_p.y - 2.0f,
                .w = static_cast<float>(usr_size.x),
                .h = 1.0f
            };
            SDL_SetRenderDrawColor(m_rend, ref.color.r, ref.color.g, ref.color.b, 255);
            SDL_RenderFillRect(m_rend, &block_rect);

            draw_p.y -= usr_size.y * 0.8f;
            draw_text(ref.user, draw_p.x + 1.0f, draw_p.y + 1.0f, c_black, w);
            draw_text(ref.user, draw_p.x, draw_p.y, lighter(ref.color), w);

            draw_p.y -= c_div_msg;
        }

        SDL_RenderPresent(m_rend);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult Display::handle(SDL_Event& event) {    
    if (event.type != SDL_EVENT_MOUSE_MOTION)
        m_draw_new = true;

    if (m_read_input) {
        switch(event.type) {
        case SDL_EVENT_QUIT:
            m_read_input = false;
            destroy_self();
            m_cond_input.notify_one();
            return SDL_APP_SUCCESS;
        case SDL_EVENT_TEXT_EDITING:
            m_input = event.edit.text;
            break;
        case SDL_EVENT_TEXT_INPUT:
            m_input += event.text.text;
            break;
        case SDL_EVENT_KEY_DOWN:
            switch(event.key.scancode) {
            case SDL_SCANCODE_ESCAPE:
                hide_kb();
                break;
            case SDL_SCANCODE_KP_BACKSPACE:
                [[ fallthrough ]];
            case SDL_SCANCODE_BACKSPACE:
                if (m_input.size())
                    m_input.pop_back();
                break;
            case SDL_SCANCODE_KP_ENTER:
                [[ fallthrough ]];
            case SDL_SCANCODE_RETURN2:
                [[ fallthrough ]];
            case SDL_SCANCODE_RETURN:
                hide_kb();
                m_read_input = false;
                m_cond_input.notify_one();
                break;
            case SDL_SCANCODE_V:
                if ((event.key.mod & SDL_KMOD_CTRL) && SDL_HasClipboardText()) {                    
                    if (char *text = SDL_GetClipboardText(); text) {
                        if (strlen(text) > 0) m_input = text;
                        SDL_free(text);
                    }
                }
                break;
            default:
                break;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            show_kb();
            break;
        default:
            break;
        }
    } else {
        switch(event.type) {
        case SDL_EVENT_QUIT:
            m_read_input = false;
            destroy_self();
            m_cond_input.notify_one();
            return SDL_APP_SUCCESS;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            m_touched = true;
            break;
        default:
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

void Display::show_kb() {
    SDL_StartTextInput(m_wind);
}

void Display::hide_kb() {
    SDL_StopTextInput(m_wind);
}

void Display::setup_self() {
    SDL_SetAppMetadata("T-Chat App", APP_VERSION, "com.lsw.tchatapp");

#if defined(SDL_PLATFORM_LINUX)
    if (!SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEO_DRIVER")) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    }
    else {
        SDL_Log("Overwrite of environment variable SDL_VIDEO_DRIVER detected. Note: Wayland may cause memory leaks, so take care!");
    }
#endif
#if defined(SDL_PLATFORM_ANDROID)
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait");
#endif


    SDL_Init(SDL_INIT_VIDEO); //  | SDL_INIT_AUDIO | SDL_INIT_EVENTS
    TTF_Init();

    SDL_CreateWindowAndRenderer("T-Chat App", c_default_app_size.x, c_default_app_size.y, c_default_app_flags, &m_wind, &m_rend);
    //SDL_SetRenderLogicalPresentation(m_sdl.renderer, c_target_app_region.x, c_target_app_region.y, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    if (!SDL_SetRenderVSync(m_rend, 1)) {
        SDL_Log("VSync cannot be enabled: %s", SDL_GetError());
    }
    
    m_font = load_font_from_mem(m_ttf_src.data(), m_ttf_src.size(), c_font_size);
    TTF_SetFontHinting(m_font, TTF_HINTING_LIGHT_SUBPIXEL);

    m_text_eng = TTF_CreateRendererTextEngine(m_rend);
    m_text = TTF_CreateText(m_text_eng, m_font, "Loading", 0);

    m_closed = false;
}

void Display::destroy_self() {
    if (m_closed) return;

    if (m_text_eng) TTF_DestroyRendererTextEngine(std::exchange(m_text_eng, nullptr));
    if (m_text) TTF_DestroyText(std::exchange(m_text, nullptr));
    if (m_font) TTF_CloseFont(std::exchange(m_font, nullptr));

    if (m_rend) SDL_DestroyRenderer(std::exchange(m_rend, nullptr));
    if (m_wind) SDL_DestroyWindow(std::exchange(m_wind, nullptr));
    
    m_closed = true;
}


SDL_Point Display::get_window_size() {
    SDL_Point p{};
    SDL_GetRenderOutputSize(m_rend, &p.x, &p.y);
    return p;
}

SDL_Point Display::get_text_size(std::string_view str) {
    SDL_Point p{};
    TTF_SetTextString(m_text, str.data(), str.size());
    TTF_GetTextSize(m_text, &p.x, &p.y);
    return p;
}

void Display::draw_text(std::string_view str, float x, float y, SDL_Color clr, int max_w) {
    TTF_SetTextString(m_text, str.data(), str.size());
    TTF_SetTextColor(m_text, clr.r, clr.g, clr.b, clr.a);
    TTF_SetTextWrapWidth(m_text, max_w > 0 ? max_w : 0);
    TTF_DrawRendererText(m_text, x, y);
}

TTF_Font* load_font_from_mem(const char* data, const size_t len, float ptsize) {
    SDL_IOStream* io = SDL_IOFromConstMem(data, len);
    return TTF_OpenFontIO(io, true, ptsize); // true = auto-close IO stream
}