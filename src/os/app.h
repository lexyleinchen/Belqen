#ifndef APP_H
#define APP_H

struct Window;

typedef Window* (*AppInitFunc)();

struct App {
    const char* name;
    AppInitFunc init;
};

#endif // APP_H