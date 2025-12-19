#include "CustomTabBar.h"
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QApplication>
#include <QLabel>
#include <QDateTime>
#include <QFile>
#include <unistd.h>

static void writeTabDebug(const QString& msg)
{
    QString path = QStringLiteral("/tmp/libresoundboard-debug.log");
    QString line = QDateTime::currentDateTime().toString(Qt::ISODate) + " [" + QString::number(getpid()) + "] " + msg + "\n";
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.close();
    }
    qDebug().noquote() << line.trimmed();
}
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QStyleOptionTab>
#include <QStyle>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QLabel>

CustomTabBar::CustomTabBar(QWidget* parent)
    : QTabBar(parent)
{
    setMovable(true);
    setAcceptDrops(true);
    connect(this, &QTabBar::tabMoved, this, &CustomTabBar::onTabMoved);
    m_dropIndicator = new QLabel(this);
    m_dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dropIndicator->setVisible(false);
    m_dropIndicator->setStyleSheet("background: rgba(0, 120, 215, 0.20); border-radius:4px;");
    m_currentDropIndex = -1;
}

void CustomTabBar::mousePressEvent(QMouseEvent* event)
{
    m_pressPos = event->pos();
    m_pressIndex = tabAt(m_pressPos);
    m_dragging = false;
    QTabBar::mousePressEvent(event);
}

void CustomTabBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_pressIndex >= 0 && !m_dragging) {
        int dist = (event->pos() - m_pressPos).manhattanLength();
        if (dist >= QApplication::startDragDistance()) {
            m_dragging = true;
            // Create pixmap of the tab
            QRect tr = tabRect(m_pressIndex);
            QPixmap pm(tr.size());
            pm.fill(Qt::transparent);
            QStyleOptionTab opt;
            initStyleOption(&opt, m_pressIndex);
            opt.rect = QRect(0,0,tr.width(), tr.height());
            QPainter p(&pm);
            style()->drawControl(QStyle::CE_TabBarTabShape, &opt, &p, this);
            style()->drawControl(QStyle::CE_TabBarTabLabel, &opt, &p, this);
            p.end();

            // Make pixmap semi-transparent to act as ghost by painting
            // the original pixmap onto a transparent pixmap with reduced opacity.
            QPixmap ghost(pm.size());
            ghost.fill(Qt::transparent);
            QPainter gp(&ghost);
            gp.setOpacity(0.5);
            gp.drawPixmap(0, 0, pm);
            gp.end();

            QMimeData* mime = new QMimeData;
            mime->setData("application/x-tab-index", QByteArray::number(m_pressIndex));

            QDrag* drag = new QDrag(this);
            drag->setMimeData(mime);
            drag->setPixmap(ghost);
            // Offset the drag pixmap so it does not cover the drop target under the cursor.
            // Some X11 compositors draw drag pixmaps opaquely; placing the pixmap to the
            // right of the cursor helps keep the highlighted tab visible.
            int hotX = qMin(12, ghost.width() / 4);
            int hotY = ghost.height() / 2;
            drag->setHotSpot(QPoint(hotX, hotY));

            // Start drag; allow move action
            Qt::DropAction act = drag->exec(Qt::MoveAction);
            Q_UNUSED(act);
        }
    }
    QTabBar::mouseMoveEvent(event);
}

void CustomTabBar::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QTabBar::mouseReleaseEvent(event);
}

void CustomTabBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData() && event->mimeData()->hasFormat("application/x-tab-index")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void CustomTabBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData() && event->mimeData()->hasFormat("application/x-tab-index")) {
        // compute drop index by midpoint and show highlight
        QPoint pos = event->position().toPoint();
        int nTabs = count();
        int to = -1;
        for (int i = 0; i < nTabs; ++i) {
            QRect r = tabRect(i);
            int midx = r.left() + r.width() / 2;
            if (pos.x() < midx) { to = i; break; }
        }
        if (to == -1) to = nTabs; // append

        // update visual indicator only if changed
        if (to != m_currentDropIndex) {
            m_currentDropIndex = to;
            if (to >= 0 && to < nTabs) {
                QRect r = tabRect(to);
                m_dropIndicator->setGeometry(r);
                m_dropIndicator->setVisible(true);
            } else if (to == nTabs && nTabs > 0) {
                QRect rlast = tabRect(nTabs - 1);
                // small vertical bar after the last tab
                QRect bar(rlast.right() + 6, rlast.top(), 8, rlast.height());
                m_dropIndicator->setGeometry(bar);
                m_dropIndicator->setVisible(true);
            } else {
                m_dropIndicator->setVisible(false);
            }
        }
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void CustomTabBar::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    m_currentDropIndex = -1;
    if (m_dropIndicator) m_dropIndicator->setVisible(false);
}

void CustomTabBar::dropEvent(QDropEvent* event)
{
    // hide indicator immediately
    m_currentDropIndex = -1;
    if (m_dropIndicator) m_dropIndicator->setVisible(false);

    if (!event->mimeData() || !event->mimeData()->hasFormat("application/x-tab-index")) {
        event->ignore();
        return;
    }
    QByteArray data = event->mimeData()->data("application/x-tab-index");
    bool ok = false;
    int from = data.toInt(&ok);
    if (!ok || from < 0 || from >= count()) {
        event->ignore();
        return;
    }
    // Use the full tab bar area and compute insertion index by comparing
    // the drop X against tab rect midpoints so drops between tabs work
    // and an explicit append (to == count()) is possible.
    QPoint pos = event->position().toPoint();
    int nTabs = count();
    int to = -1;
    for (int i = 0; i < nTabs; ++i) {
        QRect r = tabRect(i);
        int midx = r.left() + r.width() / 2;
        if (pos.x() < midx) { to = i; break; }
    }
    if (to == -1) to = nTabs; // append at end

    if (to != from) {
        // Reorder the QTabWidget pages to match the desired tab order.
        QTabWidget* tabWidget = qobject_cast<QTabWidget*>(parentWidget());
        if (tabWidget) {
            int n = tabWidget->count();
            // allow `to == n` (append at end)
            if (n > 0 && from >= 0 && from < n && to >= 0 && to <= n) {
                // capture current widgets/texts/icons
                QVector<QWidget*> widgets;
                QVector<QString> texts;
                QVector<QIcon> icons;
                widgets.reserve(n);
                texts.reserve(n);
                icons.reserve(n);
                for (int i = 0; i < n; ++i) {
                    widgets.push_back(tabWidget->widget(i));
                    texts.push_back(tabWidget->tabText(i));
                    icons.push_back(tabWidget->tabIcon(i));
                }

                // build new order: all indices except 'from', then insert 'from' at position 'to'
                QVector<int> order;
                order.reserve(n);
                for (int i = 0; i < n; ++i) if (i != from) order.push_back(i);
                if (to > (int)order.size()) to = order.size();
                order.insert(order.begin() + to, from);

                // remove tabs (widgets remain)
                for (int i = n - 1; i >= 0; --i) tabWidget->removeTab(i);

                // reinsert in new order
                for (int idx = 0; idx < (int)order.size(); ++idx) {
                    int orig = order[idx];
                    tabWidget->insertTab(idx, widgets[orig], icons[orig], texts[orig]);
                }

                // determine new index of moved tab (where 'from' ended up)
                int newIndex = -1;
                for (int idx = 0; idx < (int)order.size(); ++idx) if (order[idx] == from) { newIndex = idx; break; }
                if (newIndex >= 0) {
                    writeTabDebug(QString("CustomTabBar: reordered from=%1 to=%2").arg(from).arg(newIndex));
                    onTabMoved(from, newIndex);
                    // notify MainWindow (or other listeners) that a full reorder happened
                    emit tabOrderChanged();
                    // Make the moved tab the active tab after the reinsertions
                    // complete. Use a queued singleShot to avoid other internal
                    // QTabWidget updates clobbering our selection.
                    QTabWidget* tv = tabWidget;
                    int ni = newIndex;
                    QTimer::singleShot(0, tv, [tv, ni]() {
                        if (tv && ni >= 0 && ni < tv->count()) {
                            tv->setCurrentIndex(ni);
                            QWidget* w = tv->widget(ni);
                            if (w) w->setFocus();
                        }
                    });
                }
                event->acceptProposedAction();
                return;
            }
        }
        // fallback: move tab within the bar
        int fallbackTo = to;
        if (fallbackTo >= count()) fallbackTo = count()-1;
        moveTab(from, fallbackTo);
        writeTabDebug(QString("CustomTabBar: fallback moveTab from=%1 to=%2").arg(from).arg(fallbackTo));
        emit tabOrderChanged();
        // if we have a QTabWidget parent, make the moved tab active after
        // the move completes (queued) so internal updates don't override it.
        QTabWidget* parentTabWidget = qobject_cast<QTabWidget*>(parentWidget());
        if (parentTabWidget) {
            int idx = fallbackTo;
            if (idx < 0) idx = 0;
            QTabWidget* tv = parentTabWidget;
            QTimer::singleShot(0, tv, [tv, idx]() {
                int clamped = idx;
                if (clamped >= tv->count()) clamped = tv->count() - 1;
                if (clamped < 0) clamped = 0;
                tv->setCurrentIndex(clamped);
                QWidget* w = tv->widget(clamped);
                if (w) w->setFocus();
            });
        }
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void CustomTabBar::onTabMoved(int from, int to)
{
    // Animate a brief highlight/fade-in at the new tab position to indicate movement
    QRect tr = tabRect(to);
    if (tr.isNull()) return;

    // Create overlay label on the top-level window
    QWidget* top = window();
    QLabel* overlay = new QLabel(top);
    overlay->setText(tabText(to));
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setStyleSheet("background: rgba(255,255,255,0.9); border: 1px solid rgba(0,0,0,0.2); padding:4px;");
    QPoint globalPos = mapTo(top, tr.topLeft());
    overlay->move(globalPos);
    overlay->resize(tr.size());
    overlay->show();

    QGraphicsOpacityEffect* eff = new QGraphicsOpacityEffect(overlay);
    overlay->setGraphicsEffect(eff);
    eff->setOpacity(0.0);

    QPropertyAnimation* anim = new QPropertyAnimation(eff, "opacity", overlay);
    anim->setDuration(350);
    anim->setStartValue(0.0);
    anim->setKeyValueAt(0.6, 1.0);
    anim->setEndValue(0.0);
    QObject::connect(anim, &QPropertyAnimation::finished, overlay, [overlay, eff, anim]() {
        overlay->deleteLater();
        eff->deleteLater();
        anim->deleteLater();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
