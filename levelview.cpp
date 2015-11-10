/*
    Copyright 2015 StapleButter

    This file is part of CoinKiller.

    CoinKiller is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    CoinKiller is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along
    with CoinKiller. If not, see http://www.gnu.org/licenses/.
*/

#include "leveleditorwindow.h"
#include "levelview.h"
#include "unitsconvert.h"
#include "objectrenderer.h"

#include <QApplication>
#include <QPainter>
#include <QBrush>
#include <QColor>
#include <QRect>
#include <QRectF>
#include <QPaintEvent>
#include <QClipboard>


LevelView::LevelView(QWidget *parent, Level* level) : QWidget(parent)
{
    this->level = level;

    layerMask = 0x7; // failsafe

    zoom = 1;

    areaSelction = false;
    forbidDrag = false;
}


void LevelView::paintEvent(QPaintEvent* evt)
{
    QPainter painter(this);
    painter.scale(zoom,zoom);

    QRect drawrect(evt->rect().x()/zoom, evt->rect().y()/zoom, evt->rect().width()/zoom, evt->rect().height()/zoom);


    //qDebug("draw %d,%d %d,%d", drawrect.x(), drawrect.y(), drawrect.width(), drawrect.height());

    painter.fillRect(drawrect, QColor(119,136,153));
    tileGrid.clear();

    // Render Tiles
    for (int l = 1; l >= 0; l--)
    {
        if (!(layerMask & (1<<l)))
            continue;

        tileGrid[0xFFFFFFFF] = l+1;

        for (int i = level->objects[l].size()-1; i >= 0; i--)
        {
            const BgdatObject& obj = level->objects[l].at(i);

            // don't draw shit that is outside of the view
            // (TODO: also eliminate individual out-of-view tiles)
            if (!drawrect.intersects(QRect(obj.getx(), obj.gety(), obj.getwidth(), obj.getheight())))
                continue;

            quint16 tsid = (obj.getid() >> 12) & 0x3;
            if (level->tilesets[tsid])
            {
                level->tilesets[tsid]->drawObject(painter, tileGrid, obj.getid()&0x0FFF, obj.getx()/20, obj.gety()/20, obj.getwidth()/20, obj.getheight()/20, 1);
            }
            else
            {
                // TODO fallback
                qDebug("attempt to draw obj %04X with non-existing tileset", obj.getid());
            }
        }
    }

    painter.setRenderHint(QPainter::Antialiasing);

    // Render Locations
    for (int i = 0; i < level->locations.size(); i++)
    {
        const Location& loc = level->locations.at(i);

        QRect locrect(loc.getx(), loc.gety(), loc.getwidth(), loc.getheight());

        if (!drawrect.intersects(locrect))
            continue;

        painter.fillRect(locrect, QBrush(QColor(255,255,0,100)));

        painter.setPen(QColor(0,0,0));
        painter.drawRect(locrect);

        QString locText = QString("%1").arg(loc.getid());
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.setPen(QColor(255,255,255));
        painter.drawText(locrect.adjusted(5,5,0,0), locText);
    }

    // Render Sprites
    for (int i = 0; i < level->sprites.size(); i++)
    {
        const Sprite& spr = level->sprites.at(i);

        QRect sprRect(spr.getx()+spr.getOffsetX(), spr.gety()+spr.getOffsetY(), spr.getwidth(), spr.getheight());

        if (!drawrect.intersects(sprRect))
            continue;

        SpriteRenderer sprRend(&spr);
        sprRend.render(&painter);
    }

    // Render Entrences
    for (int i = 0; i < level->entrances.size(); i++)
    {
        const Entrance& entr = level->entrances.at(i);

        QRect entrrect(entr.getx(), entr.gety(), 20, 20);

        if (!drawrect.intersects(entrrect))
            continue;

        painter.setPen(QColor(0,0,0));

        QPainterPath path;
        path.addRoundedRect(entrrect, 2.0, 2.0);
        QColor color(182,3,3,200);
        painter.fillPath(path, color);
        painter.drawPath(path);

        QString entrText = QString("%1").arg(entr.getid());
        painter.setFont(QFont("Arial", 7, QFont::Normal));
        painter.drawText(entrrect, entrText, Qt::AlignHCenter | Qt::AlignVCenter);
    }

    // Render Paths
    for (int i = 0; i < level->paths.size(); i++)
    {
        const Path& path = level->paths.at(i);
        QList<PathNode> nodes  = path.getNodes();

        for (int j = 0; j < nodes.size() - 1; j++)
        {
            QLine pathLine(QPoint(nodes[j].getx()+10, nodes[j].gety()+10), QPoint(nodes[j+1].getx()+10, nodes[j+1].gety()+10));

            if (!drawrect.intersects(QRect(pathLine.x1(), pathLine.y1(), pathLine.x2()-pathLine.x1(), pathLine.y2()-pathLine.y1())))
                continue;

            QPen pen(QColor(0,255,20));
            pen.setWidth(2);
            painter.setPen(pen);
            painter.drawLine(pathLine);
        }

        for (int j = 0; j < nodes.size(); j++)
        {
            QRect pathrect(nodes[j].getx(), nodes[j].gety(), 20, 20);

            if (!drawrect.intersects(pathrect))
                continue;

            painter.setPen(QColor(0,0,0));

            QPainterPath painterPath;
            painterPath.addRoundedRect(pathrect, 2.0, 2.0);
            QColor color(0,255,20,200);
            painter.fillPath(painterPath, color);
            painter.drawPath(painterPath);

            QString pathText = QString("%1-%2").arg(path.getid()).arg(j+1);
            painter.setFont(QFont("Arial", 7, QFont::Normal));
            painter.drawText(pathrect, pathText, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }

    // Render Progress Paths
    for (int i = 0; i < level->progressPaths.size(); i++)
    {
        const ProgressPath& pPath = level->progressPaths.at(i);
        QList<ProgressPathNode> nodes  = pPath.getNodes();

        for (int j = 0; j < nodes.size() - 1; j++)
        {
            QLine ppathLine(QPoint(nodes[j].getx()+10, nodes[j].gety()+10), QPoint(nodes[j+1].getx()+10, nodes[j+1].gety()+10));

            if (!drawrect.intersects(QRect(ppathLine.x1(), ppathLine.y1(), ppathLine.x2()-ppathLine.x1(), ppathLine.y2()-ppathLine.y1())))
                continue;

            QPen pen(QColor(220,255,0));
            pen.setWidth(2);
            painter.setPen(pen);
            painter.drawLine(ppathLine);
        }

        for (int j = 0; j < nodes.size(); j++)
        {
            QRect ppathrect(nodes[j].getx(), nodes[j].gety(), 20, 20);

            if (!drawrect.intersects(ppathrect))
                continue;

            painter.setPen(QColor(0,0,0));

            QPainterPath path;
            path.addRoundedRect(ppathrect, 2.0, 2.0);
            QColor color(220,255,0,200);
            painter.fillPath(path, color);
            painter.drawPath(path);

            QString pPathText = QString("%1-%2").arg(pPath.getid()).arg(j+1);
            painter.setFont(QFont("Arial", 7, QFont::Normal));
            painter.drawText(ppathrect, pPathText, Qt::AlignHCenter | Qt::AlignVCenter);
        }
    }

    // Render Zones
    for (int i = 0; i < level->zones.size(); i++)
    {
        const Zone& zone = level->zones.at(i);

        QRect zonerect(zone.getx(), zone.gety(), zone.getwidth(), zone.getheight());

        if (!drawrect.intersects(zonerect))
            continue;

        painter.setPen(QColor(255,255,255));

        painter.drawRect(zonerect);

        QString zoneText = QString("Zone %1").arg(zone.getid());
        painter.setFont(QFont("Arial", 10, QFont::Normal));
        painter.drawText(zonerect.adjusted(5,5,0,0), zoneText);
    }

    // Render Selection
    for (int i = 0; i < selObjects.size(); i++)
    {
        QRect objrect(selObjects[i]->getx()+selObjects[i]->getOffsetX(), selObjects[i]->gety()+selObjects[i]->getOffsetY(), selObjects[i]->getwidth(), selObjects[i]->getheight());

        painter.setPen(QPen(QColor(255,255,255,200), 1, Qt::DotLine));
        painter.drawRect(objrect);
        painter.fillRect(objrect, QColor(255,255,255,75));
    }

    // Render Selection Area
    if (areaSelction)
    {
        painter.setPen(QPen(QColor(100,100,255,200), 1));
        painter.fillRect(selArea, QColor(100,100,255,25));
        painter.drawRect(selArea);
    }
}


void LevelView::mousePressEvent(QMouseEvent* evt)
{    
    int x = evt->x()/zoom;
    int y = evt->y()/zoom;

    if (evt->button() != Qt::LeftButton)
        return;

    forbidDrag = false;

    bool hitSelction = false;
    QList<Object*> selectedTile = selObjectsCheck(x,y,0,0,false);

    for (int i = 0; i < selObjects.size(); i++)
    {
        if (selectedTile.size() != 0 && selObjects[i] == selectedTile[0])
        {

            if (evt->modifiers() != Qt::ShiftModifier)
            {
                hitSelction = true;
                break;
            }
            // Remove from selection if clicked on object already in selection
            else
            {
                selObjects.removeAt(i);
                forbidDrag = true;
                update();
                return;
            }
        }
    }

    if (evt->modifiers() != Qt::ShiftModifier && !hitSelction) selObjects.clear();

    selObjects.append(selObjectsCheck(x, y, 0, 0, false));

    if (!hitSelction && evt->modifiers() == Qt::ShiftModifier)
        forbidDrag = true;

    // Remove doubled entrys
    for (int i = 0; i < selObjects.size(); i++)
    {
        for (int j = 0; j < selObjects.size(); j++) if (i != j && selObjects[i] == selObjects[j]) selObjects.removeAt(j);
    }

    dragX = x;
    dragY = y;
    for (int i = 0; i < selObjects.size(); i++)
    {
        selObjects[i]->setDrag(selObjects[i]->getx(), selObjects[i]->gety());
    }

    if (selObjects.size() == 0)
    {
        areaSelction = true;
        selArea = QRect(x,y,0,0);
    }

    update();
}


void LevelView::mouseMoveEvent(QMouseEvent* evt)
{    
    int x = evt->x()/zoom;
    int y = evt->y()/zoom;

    if (evt->buttons() != Qt::LeftButton) // checkme?
        return;

    if (areaSelction)
    {
        int sx = qMin(dragX,x);
        int sy = qMin(dragY,y);
        int width = qAbs(dragX-x);
        int height = qAbs(dragY-y);
        selArea = QRect(sx,sy,width,height);

        selObjects.clear();
        selObjects.append(selObjectsCheck(selArea.x(),selArea.y(),selArea.width(),selArea.height(),true));
        update();
        return;
    }

    if (forbidDrag)
        return;

    bool roundToFullTile = false;
    for (int i = 0; i < selObjects.size(); i++)
    {
        if (selObjects[i]->getType() == 0)
        {
            roundToFullTile = true;
            break;
        }
    }

    for (int i = 0; i < selObjects.size(); i++)
    {
        int finalX, finalY;

        // Rounded to next Tile
        if (roundToFullTile)
        {
            finalX = selObjects[i]->getDragX() + toNext20(x-dragX);
            finalY = selObjects[i]->getDragY() + toNext20(y-dragY);
        }

        // For Based on 16
        else
        {
            // Drag stuff freely
            if (evt->modifiers() == Qt::AltModifier)
            {
                finalX = selObjects[i]->getDragX() + toNext16Compatible(x-dragX);
                finalY = selObjects[i]->getDragY() + toNext16Compatible(y-dragY);
            }
            // Rounded to next half Tile
            else
            {
                finalX = selObjects[i]->getDragX() + toNext10(x-dragX);
                finalY = selObjects[i]->getDragY() + toNext10(y-dragY);
            }
        }

        // clamp coords
        if (finalX < 0) finalX = 0;
        else if (finalX > 0xFFFF*20) finalX = 0xFFFF*20;
        if (finalY < 0) finalY = 0;
        else if (finalY > 0xFFFF*20) finalY = 0xFFFF*20;

        selObjects[i]->setPosition(finalX, finalY);
    }

    update();
}

void LevelView::mouseReleaseEvent(QMouseEvent *evt)
{
    if (evt->buttons() == Qt::LeftButton) // checkme?
        return;

    areaSelction = false;

    update();
}

void LevelView::moveEvent(QMoveEvent *)
{
    update();
}

QList<Object*> LevelView::selObjectsCheck(int x, int y, int w, int h, bool multiSelect)
{
    QList<Object*> objects;

    bool stopChecking = false;

    // Check for Progress Path Nodes
    if (!stopChecking)
    {
        for (int p = level->progressPaths.size()-1; p >= 0; p--)
        {
            for (int i = level->progressPaths[p].getNodes().size()-1; i >= 0; i--)
            {
                ProgressPathNode& node = level->progressPaths[p].getNodeReference(i);

                if (node.clickDetection(x,y,w,h))
                {    
                    objects.append(&node);

                    if (!multiSelect)
                    {
                        stopChecking = true;
                        break;
                    }
                }
            }

            if (stopChecking) break;
        }
    }

    // Check for Path Nodes
    if (!stopChecking)
    {
        for (int p = level->paths.size()-1; p >= 0; p--)
        {
            for (int i = level->paths[p].getNodes().size()-1; i >= 0; i--)
            {
                PathNode& node = level->paths[p].getNodeReference(i);

                if (node.clickDetection(x,y,w,h))
                {
                    objects.append(&node);

                    if (!multiSelect)
                    {
                        stopChecking = true;
                        break;
                    }
                }
            }

            if (stopChecking) break;
        }
    }

    // Check for Entrances
    if (!stopChecking)
    {
        for (int i = level->entrances.size()-1; i >= 0; i--)
        {
            Entrance& entr = level->entrances[i];
            if (entr.clickDetection(x,y,w,h))
            {
                objects.append(&entr);

                if (!multiSelect)
                {
                    stopChecking = true;
                    break;
                }
            }
        }
    }

    // Check for Sprites
    if (!stopChecking)
    {
        for (int i = level->sprites.size()-1; i >= 0; i--)
        {
            Sprite& spr = level->sprites[i];
            if (spr.clickDetection(x,y,w,h))
            {
                objects.append(&spr);

                if (!multiSelect)
                {
                    stopChecking = true;
                    break;
                }
            }
        }
    }

    // Check for Tiles
    if (!stopChecking)
    {
        for (int l = 0; l < 2; l++)
        {
            if (!(layerMask & (1<<l)))
                continue;

            for (int i = level->objects[l].size()-1; i >= 0; i--)
            {
                BgdatObject& obj = level->objects[l][i];
                if (obj.clickDetection(x,y,w,h))
                {
                    objects.append(&obj);

                    if (!multiSelect)
                    {
                        stopChecking = true;
                        break;
                    }
                }
            }

            if (stopChecking) break;
        }
    }

    return objects;

}

void LevelView::saveLevel()
{
    level->save();
}

void LevelView::copy()
{
    if (selObjects.size() == 0) return;

    QString clipboardText("CoinKillerClip|");
    /*for (int i = 0; i < selObjects.size(); i++)
    {
        if (i != 0) clipboardText += ":";
        clipboardText += selObjects[i]->toText();
    }*/
    clipboardText += "|";

    QApplication::clipboard()->setText(clipboardText);
}

void LevelView::paste()
{
    QString clipboardText(QApplication::clipboard()->text());
    if (clipboardText.left(14) != "CoinKillerClip") return;

    clipboardText.remove(0, 15);
    clipboardText.chop(1);

    QStringList segments = clipboardText.split(":");

    selObjects.clear();

    update();
}

void LevelView::cut()
{
    copy();
    deleteSel();
}

void LevelView::deleteSel()
{
    update();
}
