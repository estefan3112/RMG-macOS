/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020-2026 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "OGLWidget.hpp"

#include <QGuiApplication>

#include <RMG-Core/Video.hpp>

#ifdef __APPLE__
// macOS only allows [NSOpenGLContext setView:] and [NSOpenGLContext update]
// on the main thread, which Qt calls from QOpenGLContext::makeCurrent(), so on
// macOS the emulation thread uses CGL directly to make the context current and
// to swap buffers (both are allowed on any thread). When the window changes size
// the context has to be updated on the main thread, but never while the
// emulation thread is rendering a frame, so the resize only sets a flag and
// the emulation thread lets the GUI thread do the update right after a swap
#include <QMetaObject>
#include <OpenGL/OpenGL.h>
#include <objc/message.h>
#include <objc/runtime.h>

static id get_ns_context(QOpenGLContext* context)
{
    auto* nativeContext = context->nativeInterface<QNativeInterface::QCocoaGLContext>();
    if (nativeContext == nullptr)
    {
        return nullptr;
    }
    return (id)nativeContext->nativeContext();
}

static CGLContextObj get_cgl_context(QOpenGLContext* context)
{
    id nsContext = get_ns_context(context);
    if (nsContext == nullptr)
    {
        return nullptr;
    }
    return ((CGLContextObj (*)(id, SEL))objc_msgSend)(nsContext, sel_registerName("CGLContextObj"));
}
#endif // __APPLE__

using namespace UserInterface::Widget;

OGLWidget::OGLWidget(QWidget *parent)
{
    // create window container
    this->widgetContainer = QWidget::createWindowContainer(this, parent);

    // on wayland we have to make sure that the widget
    // has a black background palette set, else
    // the window will have the theme as background color
    if (QGuiApplication::platformName() == "wayland")
    {
        QPalette blackPalette;
        blackPalette.setColor(QPalette::Window, Qt::black);
        this->widgetContainer->setAutoFillBackground(true); 
        this->widgetContainer->setPalette(blackPalette);
    }

    this->setSurfaceType(QWindow::OpenGLSurface);
    this->openGLcontext = new QOpenGLContext();
}

OGLWidget::~OGLWidget(void)
{
    this->openGLcontext->deleteLater();
}

void OGLWidget::MoveContextToThread(QThread* thread)
{
    this->GetContext()->doneCurrent();
    this->GetContext()->create();
#ifdef __APPLE__
    // attach the context to this window on the GUI thread,
    // Qt calls [NSOpenGLContext setView:] the first time the
    // context is made current on this window (see above)
    if (QThread::currentThread() == QGuiApplication::instance()->thread())
    {
        if (this->handle() == nullptr)
        {
            this->create();
        }
        if (this->GetContext()->makeCurrent(this))
        {
            this->GetContext()->doneCurrent();
        }
    }
#endif // __APPLE__
    this->GetContext()->moveToThread(thread);
}

QOpenGLContext* OGLWidget::GetContext()
{
    return this->openGLcontext;
}

bool OGLWidget::MakeCurrent(void)
{
#ifdef __APPLE__
    CGLContextObj cglContext = get_cgl_context(this->GetContext());
    return cglContext != nullptr &&
            CGLSetCurrentContext(cglContext) == kCGLNoError;
#else
    return this->GetContext()->makeCurrent(this);
#endif // __APPLE__
}

void OGLWidget::SwapBuffers(void)
{
#ifdef __APPLE__
    CGLContextObj cglContext = get_cgl_context(this->GetContext());
    if (cglContext != nullptr)
    {
        CGLFlushDrawable(cglContext);
    }

    // in between frames, let the GUI thread update the context
    // when the window has changed size, and wait until it's done
    if (this->nativeContextNeedsUpdate.exchange(false))
    {
        QMetaObject::invokeMethod(this, [this]() { this->updateNativeContext(); },
                                  Qt::BlockingQueuedConnection);
    }
#else
    this->GetContext()->swapBuffers(this);
    this->GetContext()->makeCurrent(this);
#endif // __APPLE__
}

#ifdef __APPLE__
void OGLWidget::updateNativeContext(void)
{
    id nsContext = get_ns_context(this->GetContext());
    if (nsContext == nullptr)
    {
        return;
    }

    ((void (*)(id, SEL))objc_msgSend)(nsContext, sel_registerName("update"));
}
#endif // __APPLE__

void OGLWidget::SetHideCursor(bool hide)
{
    this->setCursor(hide ? Qt::BlankCursor : Qt::ArrowCursor);
}

QWidget* OGLWidget::GetWidget(void)
{
    return this->widgetContainer;
}

void OGLWidget::resizeEvent(QResizeEvent *event)
{
    if (!this->isVisible())
    {
        return;
    }

#ifdef __APPLE__
    // the emulation thread updates the context
    // (on the GUI thread) after the next swap
    this->nativeContextNeedsUpdate = true;
#endif // __APPLE__

    if (this->timerId != 0)
    {
        this->killTimer(this->timerId);
        this->timerId = 0;
    }

    this->timerId = this->startTimer(100);

    // account for HiDPI scaling
    // see https://github.com/Rosalie241/RMG/issues/2
    this->width  = event->size().width() * this->devicePixelRatio();
    this->height = event->size().height() * this->devicePixelRatio();
}

void OGLWidget::timerEvent(QTimerEvent *event)
{
    if (!this->isVisible())
    {
        this->killTimer(this->timerId);
        this->timerId = 0;
        return;
    }

    // only remove current timer
    // when setting the video size succeeds
    if (CoreSetVideoSize(this->width, this->height))
    {
        this->killTimer(this->timerId);
        this->timerId = 0;
    }
}
