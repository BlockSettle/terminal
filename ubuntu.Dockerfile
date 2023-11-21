FROM ubuntu:22.04 as terminal_build_machine

ENV TZ="Etc/UTC"

RUN sed -Ei 's/^# deb-src /deb-src /' /etc/apt/sources.list \
    && apt update \
    # timezone thing
    && apt install -y tzdata \
    && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime \
    && echo $TZ > /etc/timezone && dpkg-reconfigure -f noninteractive tzdata \
    # main libs
    && apt install -y python3 \
    python3-pip cmake libmysqlclient-dev autoconf libtool yasm nasm libgmp3-dev libdouble-conversion-dev \
    qttools5-dev-tools libfreetype-dev libfontconfig-dev libcups2-dev xcb fuse libfuse2 \
    libx11-xcb-dev libxcb-xkb-dev libxcb-xinput-dev libxcb-sync-dev libxcb-render-util0-dev libxcb-xfixes0-dev \    
    libxcb-xinerama0-dev libxcb-randr0-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-icccm4-dev libxcb-glx0-dev libxkbcommon-x11-dev \
    libudev-dev libxi-dev libsm-dev libxrender-dev libdbus-1-dev \
    && pip install wget requests pathlib \
    # free up space
    && rm -rf /var/lib/apt/lists/* \
    && ln -s /usr/bin/python3 /usr/bin/python


WORKDIR app

COPY . .

CMD ["/app/build.sh"]


### OLD STYLE BUILDING	
#RUN python generate.py release --appimage
#
#RUN cd terminal.release \
#    && make -j$(nproc)
#
#RUN ls -la /app/build_terminal/RelWithDebInfo/bin

#RUN cd Deploy \
#    && ./deploy.sh
