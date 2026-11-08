package com.develderlost.hmbtn;

import org.libsdl.app.SDLActivity;

/** The app's single activity: SDL's activity, loading libSDL2.so and then libmain.so. */
public class HmbtnActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        // The last entry is the library that holds SDL_main.
        return new String[] {"SDL2", "main"};
    }
}
