
#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <AL/al.h>
#include <AL/alc.h>
#include "../utils/gu_error.h"

#include "../utils/math_utils.h" // used for integer types.

namespace au
{

struct Sound
{
    uint8 channels;
    int32 sampleRate;
    uint8 bitsPerSample;
    ALuint buffer;

    ~Sound();
};

class SoundSource
{
    ALuint source;

  public:
    SoundSource(const Sound &sound);

    void play();

    void stop();

    bool isPlaying();

    void setPitch(float);
    void setVolume(float);
    void setPosition(const vec3&);
    void setVelocity(const vec3&);
    void setLooping(bool);

    ~SoundSource();

};

bool getAvailableDevices(std::vector<std::string>& devicesVec, ALCdevice* device);

void init();

// todo: shutdown OpenAL device, but tbh I dont give a fuck



#define alCall(function, ...) alCallImpl(__FILE__, __LINE__, function, __VA_ARGS__)

bool check_al_errors(const std::string& filename, const std::uint_fast32_t line);

#define alcCall(function, device, ...) alcCallImpl(__FILE__, __LINE__, function, device, __VA_ARGS__)

bool check_alc_errors(const std::string& filename, const std::uint_fast32_t line, ALCdevice* device);

template<typename alcFunction, typename... Params>
auto alcCallImpl(const char* filename,
                 const std::uint_fast32_t line,
                 alcFunction function,
                 ALCdevice* device,
                 Params... params)
->typename std::enable_if_t<std::is_same_v<void,decltype(function(params...))>,bool>
{
    function(std::forward<Params>(params)...);
    return check_alc_errors(filename,line,device);
}

template<typename alcFunction, typename ReturnType, typename... Params>
auto alcCallImpl(const char* filename,
                 const std::uint_fast32_t line,
                 alcFunction function,
                 ReturnType& returnValue,
                 ALCdevice* device,
                 Params... params)
->typename std::enable_if_t<!std::is_same_v<void,decltype(function(params...))>,bool>
{
    returnValue = function(std::forward<Params>(params)...);
    return check_alc_errors(filename,line,device);
}

template<typename alFunction, typename... Params>
auto alCallImpl(const char* filename,
                const std::uint_fast32_t line,
                alFunction function,
                Params... params)
->typename std::enable_if_t<!std::is_same_v<void, decltype(function(params...))>, decltype(function(params...))>
{
    auto ret = function(std::forward<Params>(params)...);
    check_al_errors(filename, line);
    return ret;
}

template<typename alFunction, typename... Params>
auto alCallImpl(const char* filename,
                const std::uint_fast32_t line,
                alFunction function,
                Params... params)
->typename std::enable_if_t<std::is_same_v<void, decltype(function(params...))>, bool>
{
    function(std::forward<Params>(params)...);
    return check_al_errors(filename, line);
}

}
#endif //GAME_AUDIO_H
