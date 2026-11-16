Name:           kio-adb
Version:        0.3.1
Release:        1%{?dist}
Summary:        KDE KIO worker for Android devices over ADB
License:        LGPL-2.0-or-later
URL:            https://github.com/deje07/kio-adb
Source0:        %{name}-%{version}.tar

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  cmake(KF6KIO) >= 6.0
BuildRequires:  cmake(Qt6Core)
%if 0%{?suse_version}
BuildRequires:  kf6-extra-cmake-modules >= 6.0
%else
BuildRequires:  extra-cmake-modules >= 6.0
%endif
Requires:       android-tools

%description
Browse and transfer files on an Android device from Dolphin or another
KDE Frameworks 6 application using the adb: URL scheme.

%prep
%autosetup

%build
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="%{optflags}" \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
    -DKIO_ADB_EMULATOR_TEST=OFF
cmake --build build --parallel %{?_smp_build_ncpus}

%install
DESTDIR="%{buildroot}" cmake --install build

%files
%license LICENSE
%doc README.md
%{_libdir}/qt6/plugins/kf6/kio/adb.so
%{_datadir}/remoteview/adb.desktop
