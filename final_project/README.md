# Виртуальный L2 свитч

## Цель проекта

Реализовать программно основной функционал Ethernet коммутатора 2-го уровня и виртуальных портов,
позволяющих подключаться к нему любому сетевому устройству.

## Задачи
1. Создать виртуальный порт, перенаправляющий траффик с TAP устройства в сетевой сокет.
2. Разработать симулятор коммутатора 2-го уровня.
3. Добавить работу в режиме демона.
4. Реализовать дополнительный функционал (интерфейс командной строки, VLAN).

## Основные функции коммутатора второго уровня

L2 switch работает на втором уровне сетевой модели OSI, пересылая кадры Ethernet, не анализируя их содержимого.
В основе его работы лежит таблица MAC адресов.

Алгоритм работы состоит из трёх частей:
 * MAC learning;
 * MAC forwarding;
 * MAC aging.

### MAC learning
При поступлении на входной порт фрейма таблица MAC адресов просматривается, и если адрес 

Он имитирует поведение физического коммутатора, предоставляя услуги обмена кадрами Ethernet для устройств, подключенных к портам коммутатора.

Разница в том, что порты этого виртуального коммутатора могут быть подключены к устройствам по всему миру через Интернет, что делает их похожими на те, которые находятся в одной локальной сети для операционной системы.

## Дополнительные знания

### Что такое сетевой коммутатор?

Сетевой коммутатор — это сетевое оборудование, которое соединяет устройства в компьютерной сети с помощью коммутации пакетов для получения и пересылки данных на устройство назначения.

Сетевой коммутатор — это многопортовый сетевой мост, который использует MAC-адреса для пересылки данных на канальном уровне (уровень 2) модели OSI. Некоторые коммутаторы также могут пересылать данные на сетевом уровне (уровень 3), дополнительно включив функции маршрутизации. Такие коммутаторы обычно называют коммутаторами уровня 3 или многоуровневыми коммутаторами.

В этом проекте мы сосредоточимся на коммутаторах уровня 2, которые распознают и пересылают кадры Ethernet. При пересылке пакетов коммутаторы используют таблицу пересылки для поиска порта, соответствующего адресу назначения.

В коммутаторе таблица пересылки обычно называется таблицей MAC-адресов. Эта таблица хранит MAC-адреса, известные в локальной сети, и соответствующие им порты. Когда коммутатор получает кадр Ethernet, он ищет порт, соответствующий MAC-адресу назначения в таблице пересылки, и пересылает кадр данных только на этот порт, тем самым обеспечивая пересылку данных в локальной сети. Если MAC-адрес назначения отсутствует в таблице, коммутатор пересылает кадр Ethernet на все порты, кроме исходного порта, чтобы целевое устройство могло ответить и обновить таблицу пересылки.

В этом проекте мы напишем программу под названием VSwitch в качестве виртуального коммутатора для реализации функции обмена кадрами Ethernet.

### Виртуальное сетевое устройство: TAP

TAP — это тип виртуального сетевого устройства, которое может имитировать физический сетевой интерфейс, позволяя операционным системам и приложениям использовать его как физический интерфейс. Устройства TAP обычно используются для создания соединений виртуальной частной сети (VPN) между различными компьютерами для безопасной передачи данных по сетям общего пользования.

Устройство TAP реализовано в ядре операционной системы. Оно выглядит как обычный сетевой интерфейс и может использоваться в приложениях как обычная физическая сетевая карта. Когда пакеты отправляются через устройство TAP, они передаются драйверу TUN/TAP в ядре, который передает пакеты приложению. Приложение может обрабатывать пакеты и передавать их другим устройствам или сетям. Аналогично, когда приложения отправляют пакеты, они передаются драйверу TUN/TAP, который пересылает их на указанное целевое устройство или сеть.

В этом проекте устройство TAP используется для соединения клиентских компьютеров и виртуального коммутатора, обеспечивая пересылку пакетов между клиентскими компьютерами и виртуальным коммутатором.

## Архитектура системы
- Состоит из сервера (**VSwitch**) и нескольких клиентов (**VPort**s)

- Сервер (**VSwitch**) имитирует поведение физического сетевого коммутатора, предоставляя службу обмена кадрами Ethernet для каждого компьютера, подключенного к VSwitch через VPorts.
- Поддерживает таблицу MAC-адресов
|MAC|VPort|
|--|--|
11:11:11:11:11:11 | VPort-1
aa:aa:aa:aa:aa:aa | VPort-a
- Реализует службу обмена кадрами Ethernet на основе таблицы MAC-адресов

- Клиент (**VPort**) имитирует поведение порта физического коммутатора, отвечая за ретрансляцию кадров Ethernet между коммутатором и компьютером.
- VPort имеет два конца.
- Один конец подключен к компьютеру (ядру Linux) через устройство TAP.
- Другой конец подключен к VSwitch через сокет UDP.
- Отвечает за ретрансляцию пакетов между компьютером (ядром Linux) и VSwitch.
```
Ядро Linux компьютера <==[TAP]==> (Ethernet Frame) <==[UDP]==> VServer
```
- Архитектурная схема
```
+----------------------------------------------+
| VSwitch |
| |
| +----------------+---------------+ |
| | Таблица MAC | |
| |--------------------------------+ |
| | MAC | VPort | |
| |--------------------------------+ |
| | 11:11:11:11:11 | VClient-1 | |
| |--------------------------------+ |
| | aa:aa:aa:aa:aa | VClient-a | |
| +----------------+--------------+ |
| |
| ^ ^ |
 +-----------|-----------------------|----------+ +-------|--------+ +--------|-------+ | в |
[Angliyskiy](README.md)

# Sozdayte svoy sobstvennyy Zerotier

## Vvedeniye

Realizuyte L2 VPN, analogichnyy Zerotier ili virtual'nomu kommutatoru.

On imitiruyet povedeniye fizicheskogo kommutatora, predostavlyaya uslugi obmena kadrami Ethernet dlya ustroystv, podklyuchennykh k portam kommutatora.

Raznitsa v tom, chto porty etogo virtual'nogo kommutatora mogut byt' podklyucheny k ustroystvam po vsemu miru cherez Internet, chto delayet ikh pokhozhimi na te, kotoryye nakhodyatsya v odnoy lokal'noy seti dlya operatsionnoy sistemy.

## Dopolnitel'nyye znaniya

### Chto takoye setevoy kommutator?

Setevoy kommutator — eto setevoye oborudovaniye, kotoroye soyedinyayet ustroystva v komp'yuternoy seti s pomoshch'yu kommutatsii paketov dlya polucheniya i peresylki dannykh na ustroystvo naznacheniya.

Setevoy kommutator — eto mnogoportovyy setevoy most, kotoryy ispol'zuyet MAC-adresa dlya peresylki dannykh na kanal'nom urovne (uroven' 2) modeli OSI. Nekotoryye kommutatory takzhe mogut peresylat' dannyye na setevom urovne (uroven' 3), dopolnitel'no vklyuchiv funktsii marshrutizatsii. Takiye kommutatory obychno nazyvayut kommutatorami urovnya 3 ili mnogourovnevymi kommutatorami.

V etom proyekte my sosredotochimsya na kommutatorakh urovnya 2, kotoryye raspoznayut i peresylayut kadry Ethernet. Pri peresylke paketov kommutatory ispol'zuyut tablitsu peresylki dlya poiska porta, sootvetstvuyushchego adresu naznacheniya.

V kommutatore tablitsa peresylki obychno nazyvayetsya tablitsey MAC-adresov. Eta tablitsa khranit MAC-adresa, izvestnyye v lokal'noy seti, i sootvetstvuyushchiye im porty. Kogda kommutator poluchayet kadr Ethernet, on ishchet port, sootvetstvuyushchiy MAC-adresu naznacheniya v tablitse peresylki, i peresylayet kadr dannykh tol'ko na etot port, tem samym obespechivaya peresylku dannykh v lokal'noy seti. Yesli MAC-adres naznacheniya otsutstvuyet v tablitse, kommutator peresylayet kadr Ethernet na vse porty, krome iskhodnogo porta, chtoby tselevoye ustroystvo moglo otvetit' i obnovit' tablitsu peresylki.

V etom proyekte my napishem programmu pod nazvaniyem VSwitch v kachestve virtual'nogo kommutatora dlya realizatsii funktsii obmena kadrami Ethernet.

### Virtual'noye setevoye ustroystvo: TAP

TAP — eto tip virtual'nogo setevogo ustroystva, kotoroye mozhet imitirovat' fizicheskiy setevoy interfeys, pozvolyaya operatsionnym sistemam i prilozheniyam ispol'zovat' yego kak fizicheskiy interfeys. Ustroystva TAP obychno ispol'zuyutsya dlya sozdaniya soyedineniy virtual'noy chastnoy seti (VPN) mezhdu razlichnymi komp'yuterami dlya bezopasnoy peredachi dannykh po setyam obshchego pol'zovaniya.

Ustroystvo TAP realizovano v yadre operatsionnoy sistemy. Ono vyglyadit kak obychnyy setevoy interfeys i mozhet ispol'zovat'sya v prilozheniyakh kak obychnaya fizicheskaya setevaya karta. Kogda pakety otpravlyayutsya cherez ustroystvo TAP, oni peredayutsya drayveru TUN/TAP v yadre, kotoryy peredayet pakety prilozheniyu. Prilozheniye mozhet obrabatyvat' pakety i peredavat' ikh drugim ustroystvam ili setyam. Analogichno, kogda prilozheniya otpravlyayut pakety, oni peredayutsya drayveru TUN/TAP, kotoryy peresylayet ikh na ukazannoye tselevoye ustroystvo ili set'.

V etom proyekte ustroystvo TAP ispol'zuyetsya dlya soyedineniya kliyentskikh komp'yuterov i virtual'nogo kommutatora, obespechivaya peresylku paketov mezhdu kliyentskimi komp'yuterami i virtual'nym kommutatorom.

## Arkhitektura sistemy
- Sostoit iz servera (**VSwitch**) i neskol'kikh kliyentov (**VPort**s)

- Server (**VSwitch**) imitiruyet povedeniye fizicheskogo setevogo kommutatora, predostavlyaya sluzhbu obmena kadrami Ethernet dlya kazhdogo komp'yutera, podklyuchennogo k VSwitch cherez VPorts.
- Podderzhivayet tablitsu MAC-adresov
|MAC|VPort|
|--|--|
11:11:11:11:11:11 | VPort-1
aa:aa:aa:aa:aa:aa | VPort-a
- Realizuyet sluzhbu obmena kadrami Ethernet na osnove tablitsy MAC-adresov

- Kliyent (**VPort**) imitiruyet povedeniye porta fizicheskogo kommutatora, otvechaya za retranslyatsiyu kadrov Ethernet mezhdu kommutatorom i komp'yuterom.
- VPort imeyet dva kontsa.
- Odin konets podklyuchen k komp'yuteru (yadru Linux) cherez ustroystvo TAP.
- Drugoy konets podklyuchen k VSwitch cherez soket UDP.
- Otvechayet za retranslyatsiyu paketov mezhdu komp'yuterom (yadrom Linux) i VSwitch.
```
Yadro Linux komp'yutera <==[TAP]==> (Ethernet Frame) <==[UDP]==> VServer
```
- Arkhitekturnaya skhema
```
+----------------------------------------------+
| VSwitch |
| |
| +----------------+---------------+ |
| | Tablitsa MAC | |
| |--------------------------------+ |
| | MAC | VPort | |
| |--------------------------------+ |
| | 11:11:11:11:11 | VClient-1 | |
| |--------------------------------+ |
| | aa:aa:aa:aa:aa | VClient-a | |
| +----------------+--------------+ |
| |
|           ^ ^ |
        +-----------|-----------------------|----------+ +-------|--------+ +--------|-------+ |       v |
Ещё
Отправить отзыв
Максимальное количество символов: 5 000. Чтобы переводить дальше, пользуйтесь стрелками.
Есть результаты с переводом