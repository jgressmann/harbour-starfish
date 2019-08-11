%{!?qtc_qmake:%define qtc_qmake %qmake}
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}
%{?qtc_builddir:%define _builddir %qtc_builddir}

Name:       harbour-starfish
Summary:    Get your StarCraft Brood War and StarCraft II action going!
Version:    1.1.0
Release:    1
Group:      Applications/Multimedia
#Group:      Qt/Qt
License:    MIT
URL:        https://openrepos.net/content/jgressmann/starfish
Source0:    %{name}-%{version}.tar.bz2

BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(sailfishapp) >= 0.0.10
BuildRequires:  desktop-file-utils
BuildRequires:  zlib-devel


Requires:   sailfishsilica-qt5 >= 0.10.9
# This line pulls in dependency on python3
Requires:   pyotherside-qml-plugin-python3-qt5
# QML Nemo plugins used
Requires:   nemo-qml-plugin-notifications-qt5
Requires:   nemo-qml-plugin-dbus-qt5
Requires:   nemo-qml-plugin-configuration-qt5
Requires:   libkeepalive


%description
%{summary}


%files
%defattr(-,root,root,-)
%attr(0755,root,root) %{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/%{name}/qml/*
%{_datadir}/%{name}/media/*
%{_datadir}/%{name}/icons.json.gz
%{_datadir}/%{name}/classifier.json.gz
%{_datadir}/%{name}/COPYING
%{_datadir}/%{name}/COPYING.brotli
%{_datadir}/%{name}/COPYING.vodman
%{_datadir}/%{name}/brotli.bin
%{_datadir}/%{name}/icons/*
%{_datadir}/%{name}/translations/*

%define __provides_exclude_from ^%{_datadir}/.*$

%prep
%setup -q -n %{name}-%{version}

%build
%qtc_qmake5
%qtc_make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install

