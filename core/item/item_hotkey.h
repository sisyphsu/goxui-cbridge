﻿//
// Created by sulin on 2018/1/9.
//

#ifndef CLIENT_SHELL_ITEM_HOTKEY_H
#define CLIENT_SHELL_ITEM_HOTKEY_H


#include <QObject>
#include <QHotkey>

class HotKeyItem : public QObject {
Q_OBJECT
    Q_PROPERTY(QString sequence READ getSequence WRITE setSequence)
    
private:
    QString sequence;
    QHotkey *hotkey;

public:
    explicit HotKeyItem(QObject *parent = nullptr);

    QString getSequence();

    void setSequence(QString key);

    ~HotKeyItem();

private:
    void releaseHotKey();

signals:

    void activated();

};


#endif //CLIENT_SHELL_ITEM_HOTKEY_H
