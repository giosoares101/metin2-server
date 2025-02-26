FROM ubuntu:22.04 AS build
WORKDIR /app

# Set up the CMake repository
RUN apt-get update && apt-get install -y ca-certificates gpg wget
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
RUN echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null

# Update the system and install various dependencies
RUN apt-get update && \
    apt-get install -y git cmake ninja-build build-essential tar curl zip unzip pkg-config autoconf python3 \
    libdevil-dev libncurses5-dev libbsd-dev

# Arm specific
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

# Install vcpkg and the required libraries
RUN git clone https://github.com/Microsoft/vcpkg.git
RUN bash ./vcpkg/bootstrap-vcpkg.sh
RUN ./vcpkg/vcpkg install cryptopp effolkronium-random libmariadb libevent lzo fmt spdlog argon2

COPY . .

# Build the binaries
RUN mkdir build/
RUN cd build && cmake -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ..
RUN cd build && make -j $(nproc)

FROM ubuntu:22.04 AS app
WORKDIR /app

RUN apt-get update && apt-get install -y gettext python2 libdevil-dev libbsd-dev && apt-get clean

# Copy the binaries from the build stage
COPY --from=build /app/build/src/db/db /bin/db
COPY --from=build /app/build/src/game/game /bin/game
COPY --from=build /app/build/src/quest/qc /bin/qc

# Copy the game files
COPY ./gamefiles/ .

# Copy the auxiliary files
COPY ./docker/ .

# Compile the quests
RUN cd /app/data/quest && python2 make.py

# Symlink the configuration files
RUN ln -s "./conf/CMD" "CMD"
RUN ln -s ./conf/item_names_en.txt item_names.txt
RUN ln -s ./conf/item_proto.txt item_proto.txt
RUN ln -s ./conf/mob_names_en.txt mob_names.txt
RUN ln -s ./conf/mob_proto.txt mob_proto.txt

# Set up default environment variables
ENV PUBLIC_BIND_IP=0.0.0.0
ENV INTERNAL_BIND_IP=0.0.0.0

ENTRYPOINT ["/usr/bin/bash", "docker-entrypoint.sh"]
