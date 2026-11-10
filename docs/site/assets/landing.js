const viewer = document.querySelector('.image-dialog');
if (viewer && typeof viewer.showModal === 'function') {
  const photo = viewer.querySelector('img');
  const caption = viewer.querySelector('p');
  document.querySelectorAll('.image-zoom').forEach(link => {
    link.addEventListener('click', event => {
      event.preventDefault();
      const thumbnail = link.querySelector('img');
      photo.src = link.href;
      photo.alt = thumbnail.alt;
      caption.textContent = thumbnail.alt;
      viewer.showModal();
    });
  });
  viewer.querySelector('button').addEventListener('click', () => viewer.close());
  viewer.addEventListener('click', event => {
    if (event.target === viewer) {
      const bounds = viewer.getBoundingClientRect();
      if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) viewer.close();
    }
  });
}
const menu = document.querySelector('.site-menu');
menu?.querySelectorAll('a').forEach(link => link.addEventListener('click', () => { menu.open = false; }));
