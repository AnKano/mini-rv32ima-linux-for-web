const button = document.getElementById("popupButton");
if (!button) throw 'popup button not found!';

const popupContainer = document.getElementById("popup");
if (!popupContainer) throw 'popup not found!';

button.addEventListener('click', () => {
    popupContainer.style.display = (popupContainer.style.display != 'block') ? 'block' : 'none';
});
