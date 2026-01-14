#include "mainwindow.h"
#include <QTcpSocket>

void MainWindow::addPeer(const QString &name, const QString &ip, quint16 tcpPort) {
    for (int i = 0; i < discoveredPeers.size(); ++i) {
        if (discoveredPeers[i].ip == ip) {
            discoveredPeers[i].name = name;
            discoveredPeers[i].lastDiscover = QDateTime::currentDateTime();
            discoveredPeers[i].tcpPort = tcpPort;
            if(state == Disconnected) emit updatePeerList();
            return;
        }
    }

    PeerInfo newPeer;
    newPeer.name = name;
    newPeer.ip = ip;
    newPeer.tcpPort = tcpPort;
    newPeer.lastDiscover = QDateTime::currentDateTime();

    discoveredPeers.append(newPeer);
    if(state == Disconnected) emit updatePeerList();
}

void MainWindow::removeInactivePeer() {
    for (int i = 0; i < discoveredPeers.size(); ++i) {
        const PeerInfo& tempPeer = discoveredPeers[i];
        if (tempPeer.lastDiscover.addSecs(30) < QDateTime::currentDateTime())
            discoveredPeers.removeAt(i);
    }
}
