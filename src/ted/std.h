/*
Ted, a simple text editor/IDE.

Copyright 2012, Blitz Research Ltd.

See LICENSE.TXT for licensing terms.
*/

#ifndef STD_H
#define STD_H

#include <QtGui/QtGui>

static QString textFileTypes=";txt;";
static QString codeFileTypes=";monkey;bmx;cpp;java;js;as;cs;py;";

inline bool isDigit( QChar ch ){
    return (ch>='0' && ch<='9');
}

inline bool isAlpha( QChar ch ){
    return (ch>='a' && ch<='z') || (ch>='A' && ch<='Z') || ch=='_';
}

inline bool isIdent( QChar ch ){
    return isAlpha(ch) || isDigit(ch);
}

QString fixPath( QString path );

QString stripDir( const QString &path );

QString extractDir( const QString &path );

QString extractExt( const QString &path );

bool removeDir( const QString &path );

void replaceTabWidgetWidget( QTabWidget *tabWidget,int index,QWidget *widget );

bool isUrl( const QString &path );

#endif // STD_H
