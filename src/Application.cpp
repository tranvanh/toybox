#include "Toybox/Application.h"
#include "Toybox/Logger.h"

TOYBOX_NAMESPACE_BEGIN

Application::~Application() {
    // Destruction is the final lifecycle boundary; make it safe even when the
    // caller forgot to stop explicitly.
    isRunning = false;
    mThreadPool.stop();
}

void Application::run() {
    isRunning = true;
    mThreadPool.run();
}

void Application::stop() {
    isRunning = false;
    mThreadPool.stop();
}

void Application::runBackgroundTask(std::function<void()> f) {
    mThreadPool.addTask(std::move(f));
}

TOYBOX_NAMESPACE_END
