%{!?qtc_qmake:%define qtc_qmake %qmake}
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}
%{?qtc_builddir:%define _builddir %qtc_builddir}

Name:       harbour-starfish
Summary:    Get your StarCraft II and Brood War action going!
Version:    1.0.0
Release:    1
Group:      Applications/Multimedia
#Group:      Qt/Qt
License:    MIT
#URL:        http://foo.bar
Source0:    %{name}-%{version}.tar.bz2

BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(sailfishapp) >= 0.0.10
BuildRequires:  desktop-file-utils
BuildRequires:  libvodman-devel >= 1.0.3
BuildRequires:  vodman-service >= 1.0.3
BuildRequires:  zlib-devel



Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   libvodman >= 1.0.3
Requires:   vodman-service >= 1.0.3
#Requires:   libstarfish = %{version}
Requires:   zlib


%description
%{summary}


%files
%defattr(-,root,root,-)
%attr(0755,root,root) %{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_datadir}/%{name}/qml/*
%{_datadir}/%{name}/xml/*
%{_datadir}/%{name}/media/*
%{_datadir}/%{name}/icons.json.gz
%{_datadir}/%{name}/classifier.json.gz
%{_datadir}/%{name}/COPYING
#%{_datadir}/%{name}/icons/*


#%package -n libstarfish
#Summary: starfish library.
#Group: Development/Libraries
#Provides: libstarfish = %{version}

#%description -n libstarfish
#%{summary}

#%files -n libstarfish
#%defattr(-,root,root,-)
#%{_libdir}/*

#%package -n starfish-tools
#Summary: starfish tools.
#Group: Development/Libraries
#Requires:   libstarfish = %{version}

#%description -n starfish-tools
#%{summary}

#%files -n starfish-tools
#%defattr(-,root,root,-)
#%attr(0755,root,root) %{_bindir}/starfish-fetch



%prep
%setup -q -n %{name}-%{version}

%build
%qtc_qmake5
%qtc_make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%qmake5_install


#%post -n libstarfish
#/sbin/ldconfig

#%postun -n libstarfish
#/sbin/ldconfig
