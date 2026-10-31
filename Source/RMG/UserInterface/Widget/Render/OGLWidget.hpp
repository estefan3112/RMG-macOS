/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020-2026 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef OGLWIDGET_HPP
#define OGLWIDGET_HPP

#include <QThread>
#include <QResizeEvent>
#include <QWindow>
#include <QTimerEvent>
#include <QOpenGLContext>
#include <QWidget>
#ifdef __APPLE__
#include <atomic>
#endif // __APPLE__

namespace UserInterface
{
namespace Widget
{
class OGLWidget : public QWindow
{
  public:
    OGLWidget(QWidget *);
    ~OGLWidget(void);

    void MoveContextToThread(QThread* thread);
    QOpenGLContext* GetContext();

    // makes the context current on this window and swaps
    // its buffers, from the thread that owns the context
    bool MakeCurrent(void);
    void SwapBuffers(void);

    void SetHideCursor(bool hide);

    QWidget* GetWidget(void);

  protected:
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;
    void timerEvent(QTimerEvent *) Q_DECL_OVERRIDE;

  private:
#ifdef __APPLE__
    void updateNativeContext(void);
    std::atomic<bool> nativeContextNeedsUpdate{false};
#endif // __APPLE__

    QWidget* widgetContainer      = nullptr;
    QOpenGLContext* openGLcontext = nullptr;
    int width                     = 0;
    int height                    = 0;
    int timerId                   = 0;
};
} // namespace Widget
} // namespace UserInterface

#endif // OGLWIDGET_HPP