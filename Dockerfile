# Derleme + test + ekran görüntüsü ortamı (CI'daki ubuntu-24.04 ile aynı paketler)
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build pkg-config git ca-certificates \
      qt6-base-dev qt6-declarative-dev \
      qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
      qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtquick-shapes \
      qml6-module-qtquick-dialogs qml6-module-qtqml-workerscript qml6-module-qtqml \
      qml6-module-qt-labs-folderlistmodel \
      libgtest-dev \
      libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
      gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
      gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-tools \
      fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
