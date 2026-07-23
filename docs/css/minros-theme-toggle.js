/* minros — sade açık/koyu tema toggle'ı.
 *
 * Kural (kullanıcı isteği):
 *   • Varsayılan = sistem teması (prefers-color-scheme).
 *   • Butona basınca seçilen tema KALICI olur (localStorage) ve sayfalar arası
 *     yaşar → sidebar linkleri tam sayfa yenilese bile state korunur.
 *   • Explicit seçim varken sistem teması değişse bile seçim sabit kalır.
 *   • Explicit seçim yokken tema sistemi canlı izler.
 *
 * FOUC yok: tema, <head> içinde ilk boyamadan ÖNCE senkron uygulanır.
 * doxygen-awesome ile uyum: html.dark-mode / html.light-mode class'ları;
 * ikisi de yoksa CSS'in @media(prefers-color-scheme) kuralı devreye girer.
 *
 * NOT: Kalıcılık localStorage gerektirir → HTTP (GitHub Pages / yerel sunucu)
 * ya da Chromium tabanlı tarayıcılarda file:// üzerinde çalışır. Firefox
 * file://'ı her dosyayı ayrı origin sayıp izole eder; orada seçim sayfa
 * değişince taşınmaz (deploy'da sorun değildir).
 */
(function () {
    "use strict";
    var KEY = "minros-theme";  // "dark" | "light" | (yok) = sistem

    function mq() { return window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)"); }
    function systemDark() { var m = mq(); return !!(m && m.matches); }
    function stored() { try { return localStorage.getItem(KEY); } catch (e) { return null; } }
    function save(v) { try { localStorage.setItem(KEY, v); } catch (e) {} }

    // Efektif tema koyu mu? (buton ikonu ve tıklama yönü için)
    function effectiveDark() {
        var s = stored();
        if (s === "dark") return true;
        if (s === "light") return false;
        return systemDark();
    }

    // Class'ları localStorage'a göre ayarla (yoksa temizle → sistem).
    function apply() {
        var el = document.documentElement, s = stored();
        el.classList.remove("dark-mode", "light-mode");
        if (s === "dark") el.classList.add("dark-mode");
        else if (s === "light") el.classList.add("light-mode");
    }

    // ── 1) Temayı HEMEN uygula (FOUC'suz) ──────────────────────────────────
    apply();

    // ── SVG ikonlar (fonts.google.com, Apache-2.0) ─────────────────────────
    var SUN = '<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24" fill="#FCBF00"><path d="M12,7c-2.76,0-5,2.24-5,5s2.24,5,5,5s5-2.24,5-5S14.76,7,12,7z M2,13l2,0c0.55,0,1-0.45,1-1s-0.45-1-1-1l-2,0c-0.55,0-1,0.45-1,1S1.45,13,2,13z M20,13l2,0c0.55,0,1-0.45,1-1s-0.45-1-1-1l-2,0c-0.55,0-1,0.45-1,1S19.45,13,20,13z M11,2v2c0,0.55,0.45,1,1,1s1-0.45,1-1V2c0-0.55-0.45-1-1-1S11,1.45,11,2z M11,20v2c0,0.55,0.45,1,1,1s1-0.45,1-1v-2c0-0.55-0.45-1-1-1C11.45,19,11,19.45,11,20z M5.99,4.58c-0.39-0.39-1.03-0.39-1.41,0c-0.39,0.39-0.39,1.03,0,1.41l1.06,1.06c0.39,0.39,1.03,0.39,1.41,0s0.39-1.03,0-1.41L5.99,4.58z M18.36,16.95c-0.39-0.39-1.03-0.39-1.41,0c-0.39,0.39-0.39,1.03,0,1.41l1.06,1.06c0.39,0.39,1.03,0.39,1.41,0c0.39-0.39,0.39-1.03,0-1.41L18.36,16.95z M19.42,5.99c0.39-0.39,0.39-1.03,0-1.41c-0.39-0.39-1.03-0.39-1.41,0l-1.06,1.06c-0.39,0.39-0.39,1.03,0,1.41s1.03,0.39,1.41,0L19.42,5.99z M7.05,18.36c0.39-0.39,0.39-1.03,0-1.41c-0.39-0.39-1.03-0.39-1.41,0l-1.06,1.06c-0.39,0.39-0.39,1.03,0,1.41s1.03,0.39,1.41,0L7.05,18.36z"/></svg>';
    var MOON = '<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24" fill="#FE9700"><path d="M12,3c-4.97,0-9,4.03-9,9s4.03,9,9,9s9-4.03,9-9c0-0.46-0.04-0.92-0.1-1.36c-0.98,1.37-2.58,2.26-4.4,2.26c-2.98,0-5.4-2.42-5.4-5.4c0-1.81,0.89-3.42,2.26-4.4C12.92,3.04,12.46,3,12,3L12,3z"/></svg>';

    var btn = null;
    function updateIcon() { if (btn) btn.innerHTML = effectiveDark() ? SUN : MOON; }

    function onClick() {
        // Buton = tema. Tıklayınca efektif temanın tersini KALICI seç.
        save(effectiveDark() ? "light" : "dark");
        apply();
        updateIcon();
    }

    function makeButton() {
        var b = document.createElement("minros-theme-toggle");
        b.title = "Açık/Koyu tema";
        b.setAttribute("role", "button");
        b.setAttribute("tabindex", "0");
        b.onclick = onClick;
        b.onkeydown = function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); onClick(); } };
        return b;
    }

    // Doxygen arama kutusunu (#MSearchBox) menü JS'iyle SONRADAN kurar; ayrıca
    // pencere yeniden boyutlanınca (mobil/masaüstü) yeniden kurar. Bu yüzden
    // kutunun yanına butonu her kurulumda (yeniden) iliştir.
    function attach() {
        var box = document.getElementById("MSearchBox");
        if (!box) return false;
        var parent = box.parentNode;
        // Zaten bu konteynerde varsa dokunma; değilse (yeniden kurulmuş) taşı/ekle.
        if (btn && btn.parentNode === parent) return true;
        if (!btn) btn = makeButton();
        parent.appendChild(btn);
        updateIcon();
        return true;
    }

    // Arama kutusu hazır olana kadar kısa süre yokla (maks ~3 sn).
    function pollAttach() {
        if (attach()) return;
        var tries = 0;
        var iv = setInterval(function () {
            if (attach() || ++tries > 60) clearInterval(iv);
        }, 50);
    }

    // ── 2) Sistem teması değişince: explicit seçim yoksa canlı izle ─────────
    var m = mq();
    if (m) m.addEventListener("change", function () {
        if (!stored()) { apply(); updateIcon(); }  // class yok → media query halleder, sadece ikon
    });

    // ── 3) Butonu arama kutusunun yanına ekle (hazır olunca) ────────────────
    if (document.readyState !== "loading") pollAttach();
    else document.addEventListener("DOMContentLoaded", pollAttach);

    // ── 4) Doxygen, mobil/masaüstü geçişinde arama kutusunu YENİDEN kurup
    //      butonumuzu siliyor. Tek bir resize dinleyicisi zamanlama sırası
    //      yüzünden güvenilmez; bunun yerine DOM'u gözleyip kutu her
    //      (yeniden) oluştuğunda butonu geri iliştir. attach() zaten
    //      yerindeyse no-op → sonsuz döngü yok. ────────────────────────────
    if (window.MutationObserver) {
        new MutationObserver(function () { attach(); })
            .observe(document.documentElement, { childList: true, subtree: true });
    } else {
        window.addEventListener("resize", attach);  // eski tarayıcı yedeği
    }
})();
